#include "crow.h"
#include "cairo.h"

#include "cairo_mono_embed.h"

#include <sys/time.h>

#define MIN(X, Y) (((X) < (Y)) ? (X) : (Y))
#define MAX(X, Y) (((X) > (Y)) ? (X) : (Y))

static MonoDomain *domain;
static MonoAssembly *assembly;
static MonoImage *image;
static MonoClass *klass;
static MonoObject *objIface;
static MonoMethod *loadiface = NULL, *resize = NULL, *update = NULL,
                    *mouseMove = NULL, *mouseUp = NULL, *mouseDown = NULL,
                    *keyPress = NULL, *keyDown = NULL, *keyUp = NULL, *notifyVC;


static pthread_t crowThread;
pthread_mutex_t crow_load_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t crow_update_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t crow_event_mutex = PTHREAD_MUTEX_INITIALIZER;

static uint8_t loadFlag = FALSE; //true when a file is triggered to be loaded
static cairo_rectangle_int_t crowBuffBounds;
static char* fileName = NULL;

volatile uint8_t* crowBuffer = NULL;
volatile uint32_t dirtyOffset = 0;
volatile uint32_t dirtyLength = 0;

static CrowEvent * crow_evt_queue_start = NULL;
static CrowEvent * crow_evt_queue_end = NULL;

CrowEvent* crow_evt_create(uint32_t eventType, uint32_t data0, uint32_t data1){
    CrowEvent* pEvt = (CrowEvent*)malloc(sizeof(CrowEvent));
    pEvt->eventType = eventType;
    pEvt->data0 = data0;
    pEvt->data1 = data1;
    pEvt->pNext = NULL;
    return pEvt;
}

void crow_evt_enqueue(CrowEvent *pEvt){
    pthread_mutex_lock(&crow_event_mutex);
    if (crow_evt_queue_end){
        crow_evt_queue_end->pNext = pEvt;
        crow_evt_queue_end = pEvt;
    }else{
        crow_evt_queue_start = pEvt;
        crow_evt_queue_end = pEvt;
    }
    pthread_mutex_unlock(&crow_event_mutex);
}

CrowEvent* crow_evt_dequeue(){
    pthread_mutex_lock(&crow_event_mutex);
    CrowEvent* next = crow_evt_queue_start;
    if (next){
        if (crow_evt_queue_start->pNext)
            crow_evt_queue_start = crow_evt_queue_start->pNext;
        else
            crow_evt_queue_start = crow_evt_queue_end = NULL;
    }
    pthread_mutex_unlock(&crow_event_mutex);
    return next;
}

bool crow_evt_pending () {
    return (bool)(crow_evt_queue_start!=NULL);
}

void printException (MonoObject* exc){
    MonoMethodDesc* mdesc = mono_method_desc_new (":ToString()", FALSE);
    MonoClass* klass = mono_get_exception_class();
    MonoMethod* toString = mono_method_desc_search_in_class(mdesc,klass);

    MonoString* str = (MonoString*)mono_runtime_invoke (toString, exc, NULL, NULL);
    char* p = mono_string_to_utf8 (str);
    printf ("Error: %s\n", p);
    mono_free (p);
}

void _crow_buffer_resize (uint32_t width, uint32_t height){
    crow_lock_update_mutex();
    crowBuffBounds.width = width;
    crowBuffBounds.height = height;
    if(crowBuffer)
        free(crowBuffer);
    size_t buffSize = (size_t)(crowBuffBounds.width*crowBuffBounds.height*4);
    crowBuffer = (uint8_t*)malloc(buffSize);
    /*printf("resize: (%d,%d), bmpPtr:%lu, length=%lu\n",width,height,
           dirtyBmp, dirtyBmpSize);
    fflush(stdout);*/
    dirtyLength = dirtyOffset = 0;
    crow_release_update_mutex();
}

void crow_load () {
    while(loadFlag)
        continue;
    pthread_mutex_lock(&crow_load_mutex);
    loadFlag = TRUE;
    pthread_mutex_unlock(&crow_load_mutex);
}

void crow_lock_update_mutex(){
    pthread_mutex_lock(&crow_update_mutex);
}
void crow_release_update_mutex(){
    pthread_mutex_unlock(&crow_update_mutex);
}

void crow_mono_update (cairo_region_t* reg, MonoArray* bmp){
    crow_lock_update_mutex();

    cairo_rectangle_int_t dirtyRect = {};
    cairo_region_get_extents(reg, &dirtyRect);

    if (dirtyRect.x < 0)
        dirtyRect.x = 0;
    if (dirtyRect.y < 0)
        dirtyRect.y = 0;
    if (dirtyRect.width + dirtyRect.x > crowBuffBounds.width)
        dirtyRect.width = crowBuffBounds.width - dirtyRect.x;
    if (dirtyRect.height + dirtyRect.y > crowBuffBounds.height)
        dirtyRect.height = crowBuffBounds.height - dirtyRect.y;

    uint8_t* ptrBmp = (uint8_t*)mono_array_addr(bmp,uint8_t,0);
    int stride = crowBuffBounds.width * 4;
    uint32_t newOffset = dirtyRect.y * stride;
    uint32_t newLength = dirtyRect.height * stride;
    uint32_t dirtyRight = dirtyOffset+dirtyLength;

    dirtyOffset = MIN(newOffset, dirtyOffset);
    dirtyLength = MAX(newOffset+newLength, dirtyRight);

    /*printf("update: (%d,%d,%d,%d),offset=%d, length=%d, bmpPtr:%lu, dirtyPtr=%lu\n",dirtyRect.x,
           dirtyRect.y,dirtyRect.width,dirtyRect.height,dirtyOffset,dirtyLength,
           crowBuffer, ptrBmp);
    fflush(stdout);*/

    memcpy(crowBuffer + dirtyOffset, ptrBmp + dirtyOffset, dirtyLength);

    crow_release_update_mutex();
}

void crow_notify_value_changed(char* member, char* value){
    MonoObject *exception = NULL;
    MonoString *strVal = mono_string_new(domain, value);
    MonoString *strMemb = mono_string_new(domain, member);
    void* args[2];
    args[0] = strMemb;
    args[1] = strVal;
    mono_runtime_invoke (notifyVC, objIface, args, &exception);
    if (exception)
        printException(exception);
}
void crow_notify_value_changed_d(char* member, double value){
    MonoObject *exception = NULL;
    MonoString *strMemb = mono_string_new(domain, member);
    MonoObject *val = mono_value_box(domain,mono_get_double_class(), &value);

    void* args[2];
    args[0] = strMemb;
    args[1] = val;
    mono_runtime_invoke (notifyVC, objIface, args, &exception);
    if (exception)
        printException(exception);
}
void main_loop(){
    int cpt = 0;
    while(1){
        cpt++;
        MonoObject *exception = NULL;

        struct timeval t1, t2;
        double elapsedTime;
        gettimeofday(&t1, NULL);

        if (crow_evt_pending()){
            CrowEvent* evt = crow_evt_dequeue();
            if (evt->eventType & CROW_MOUSE_EVT){
                if (evt->eventType == CROW_MOUSE_MOVE){
                    void* args[2];
                    args[0] = &evt->data0;
                    args[1] = &evt->data1;
                    mono_runtime_invoke (mouseMove, objIface, args, &exception);
                }else if (evt->eventType == CROW_MOUSE_DOWN){
                    void* args[1];
                    args[0] = &evt->data0;
                    mono_runtime_invoke (mouseDown, objIface, args, &exception);
                }else if (evt->eventType == CROW_MOUSE_UP){
                    void* args[1];
                    args[0] = &evt->data0;
                    mono_runtime_invoke (mouseUp, objIface, args, &exception);
                }
            }else if (evt->eventType & CROW_KEY_EVT){
                void* args[1];
                args[0] = &evt->data1;
                //args[1] = &evt->data1;
                if (evt->eventType == CROW_KEY_DOWN)
                    mono_runtime_invoke (keyDown, objIface, args, &exception);
                else if (evt->eventType == CROW_KEY_UP)
                    mono_runtime_invoke (keyUp, objIface, args, &exception);
                else if (evt->eventType == CROW_KEY_PRESS){
                    args[0] = &evt->data0;
                    mono_runtime_invoke (keyPress, objIface, args, &exception);
                }
            }else if (evt->eventType & CROW_RESIZE){
                _crow_buffer_resize(evt->data0,evt->data1);
                void* args[1];
                args[0] = &crowBuffBounds;
                mono_runtime_invoke (resize, objIface, args, &exception);
            }
            free(evt);
            if (exception)
                printException(exception);
        }
        if (loadFlag){
            exception = NULL;
            pthread_mutex_lock(&crow_load_mutex);
            //MonoString *str = mono_string_new (domain, "/mnt/devel/gts/libvk/crow/Tests/Interfaces/GraphicObject/0.crow");
            MonoString *str = mono_string_new (domain, "/mnt/devel/gts/libvk/crow/Tests/Interfaces/Divers/0.crow");
            //MonoString *str = mono_string_new (domain, "/mnt/devel/gts/libvk/crow/Tests/Interfaces/TemplatedControl/testSpinner.crow");
            mono_runtime_invoke (loadiface, objIface, &str, &exception);
            loadFlag = FALSE;
            pthread_mutex_unlock(&crow_load_mutex);
            if (exception)
                printException(exception);
        }

        exception = NULL;
        mono_runtime_invoke (update, objIface, NULL, &exception);
        if (exception)
            printException(exception);

        gettimeofday(&t2, NULL);
        elapsedTime = (t2.tv_sec - t1.tv_sec) * 1000.0;      // sec to ms
        elapsedTime += (t2.tv_usec - t1.tv_usec) / 1000.0;
        char strElapsed[100];
        sprintf(strElapsed, "%lf", elapsedTime);
        crow_notify_value_changed("fps", strElapsed);
        crow_notify_value_changed_d("fpsMin", elapsedTime);
        cpt=0;
    }
}

void* crow_thread (void *arg) {
    int retval;
    const char *crowDllPath = "Crow.dll";

    domain = mono_jit_init (crowDllPath);
    mono_config_parse ("/etc/mono/config");
    assembly = mono_domain_assembly_open (domain, crowDllPath);

    if (!assembly)
        return EXIT_FAILURE;

    image = mono_assembly_get_image (assembly);

    mono_add_internal_call ("Crow.Interface::crow_mono_update", crow_mono_update);

    embed_cairo_init();

    klass = mono_class_from_name (image, "Crow", "Interface");
    if (!klass) {
        fprintf (stderr, "Can't find Interface in assembly %s\n", mono_image_get_filename (image));
        exit (1);
    }

    MonoMethod *m = NULL,
            *ctor = NULL;

    void* iter = NULL;
    while ((m = mono_class_get_methods (klass, &iter))) {
        if (strcmp (mono_method_get_name (m), ".ctor") == 0)
            ctor = m;
        else if (strcmp (mono_method_get_name (m), "LoadInterface") == 0)
            loadiface = m;
        else if (strcmp (mono_method_get_name (m), "Update") == 0)
            update = m;
        else if (strcmp (mono_method_get_name (m), "ProcessResize") == 0)
            resize = m;
        else if (strcmp (mono_method_get_name (m), "ProcessMouseMove") == 0)
            mouseMove = m;
        else if (strcmp (mono_method_get_name (m), "ProcessMouseButtonDown") == 0)
            mouseDown = m;
        else if (strcmp (mono_method_get_name (m), "ProcessMouseButtonUp") == 0)
            mouseUp = m;
        else if (strcmp (mono_method_get_name (m), "ProcessKeyUp") == 0)
            keyUp = m;
        else if (strcmp (mono_method_get_name (m), "ProcessKeyDown") == 0)
            keyDown = m;
        else if (strcmp (mono_method_get_name (m), "ProcessKeyPress") == 0)
            keyPress = m;
        else if (strcmp (mono_method_get_name (m), "NotifyValueChanged") == 0)
            notifyVC = m;
    }

    objIface = mono_object_new (domain, klass);
    //mono_runtime_object_init (obj);
    MonoObject *exc = NULL;
    mono_runtime_invoke (ctor, objIface, NULL, &exc);
    if (exc)
        printException(exc);

    main_loop();

    retval = mono_environment_exitcode_get ();

    mono_jit_cleanup (domain);

    pthread_exit(retval);
}



void crow_init () {
    if(pthread_create(&crowThread, NULL, crow_thread, NULL) == -1) {
        perror("pthread_create");
        exit (EXIT_FAILURE);
    }
}

