#include "crow.h"
#include "cairo.h"

#include "cairo_mono_embed.h"

#include <sys/time.h>

#define MIN(X, Y) (((X) < (Y)) ? (X) : (Y))
#define MAX(X, Y) (((X) > (Y)) ? (X) : (Y))

static MonoDomain *domainCrow;
static MonoAssembly *assemblyCrow;
static MonoImage *imageCrow;
static MonoClass *clsIface;
static MonoObject *objIface;
static MonoMethod *loadiface = NULL, *resize = NULL, *update = NULL,
                    *mouseMove = NULL, *mouseUp = NULL, *mouseDown = NULL,
                    *keyPress = NULL, *keyDown = NULL, *keyUp = NULL, *notifyVC;


static pthread_t crowThread;
pthread_mutex_t crow_update_mutex = PTHREAD_MUTEX_INITIALIZER;

static cairo_rectangle_int_t crowBuffBounds;
volatile uint8_t* crowBmp = NULL;
volatile uint32_t dirtyOffset = 0;
volatile uint32_t dirtyLength = 0;

void _search_iface_methods () {
    MonoMethod *m = NULL,
            *ctor = NULL;

    void* iter = NULL;
    while ((m = mono_class_get_methods (clsIface, &iter))) {
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
}

void _printException (MonoObject* exc){
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
    if(crowBmp)
        free(crowBmp);
    size_t buffSize = (size_t)(crowBuffBounds.width*crowBuffBounds.height*4);
    crowBmp = (uint8_t*)malloc(buffSize);
    /*printf("resize: (%d,%d), bmpPtr:%lu, length=%lu\n",width,height,
           dirtyBmp, dirtyBmpSize);
    fflush(stdout);*/
    dirtyLength = dirtyOffset = 0;
    crow_release_update_mutex();
}
void _crow_notify_value_changed(char* member, char* value){
    MonoObject *exception = NULL;
    MonoString *strVal = mono_string_new(domainCrow, value);
    MonoString *strMemb = mono_string_new(domainCrow, member);
    void* args[2];
    args[0] = strMemb;
    args[1] = strVal;
    mono_runtime_invoke (notifyVC, objIface, args, &exception);
    if (exception)
        _printException(exception);
}
void _crow_notify_value_changed_d(char* member, double value){
    MonoObject *exception = NULL;
    MonoString *strMemb = mono_string_new(domainCrow, member);
    MonoObject *val = mono_value_box(domainCrow,mono_get_double_class(), &value);

    void* args[2];
    args[0] = strMemb;
    args[1] = val;
    mono_runtime_invoke (notifyVC, objIface, args, &exception);
    if (exception)
        _printException(exception);
}

void _crow_mono_update (cairo_region_t* reg, MonoArray* bmp){
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

    memcpy(crowBmp + dirtyOffset, ptrBmp + dirtyOffset, dirtyLength);

    crow_release_update_mutex();
}
void _main_loop(){
    int cpt = 0;
    struct timeval lastFrame, frame;
    gettimeofday(&lastFrame, NULL);
    while(1){
        cpt++;
        MonoObject *exception = NULL;

        if (crow_evt_pending()){
            CrowEvent* evt = crow_evt_dequeue();
            if (evt->eventType & CROW_MOUSE_EVT){
                if (evt->eventType == CROW_MOUSE_MOVE){
                    void* args[2];
                    args[0] = &evt->data0.int32;
                    args[1] = &evt->data1.int32;
                    mono_runtime_invoke (mouseMove, objIface, args, &exception);
                }else if (evt->eventType == CROW_MOUSE_DOWN){
                    void* args[1];
                    args[0] = &evt->data0.int32;
                    mono_runtime_invoke (mouseDown, objIface, args, &exception);
                }else if (evt->eventType == CROW_MOUSE_UP){
                    void* args[1];
                    args[0] = &evt->data0.int32;
                    mono_runtime_invoke (mouseUp, objIface, args, &exception);
                }
            }else if (evt->eventType & CROW_KEY_EVT){
                void* args[1];
                args[0] = &evt->data0.int32;
                //args[1] = &evt->data1;
                if (evt->eventType == CROW_KEY_DOWN)
                    mono_runtime_invoke (keyDown, objIface, args, &exception);
                else if (evt->eventType == CROW_KEY_UP)
                    mono_runtime_invoke (keyUp, objIface, args, &exception);
                else if (evt->eventType == CROW_KEY_PRESS){
                    args[0] = &evt->data0.int32;
                    mono_runtime_invoke (keyPress, objIface, args, &exception);
                }
            }else if (evt->eventType == CROW_RESIZE){
                _crow_buffer_resize(evt->data0.int32,evt->data1.int32);
                void* args[1];
                args[0] = &crowBuffBounds;
                mono_runtime_invoke (resize, objIface, args, &exception);
            }else if (evt->eventType == CROW_LOAD){
                MonoString *str = mono_string_new (domainCrow, evt->data0.str);
                void* args[1];
                args[0] = str;
                crow_lock_update_mutex();
                mono_runtime_invoke (loadiface, objIface, args, &exception);
                crow_release_update_mutex();
            }
            free(evt);
            if (exception)
                _printException(exception);
        }

        gettimeofday(&frame, NULL);
        double elapsed = (frame.tv_sec - lastFrame.tv_sec) * 1000.0;      // sec to ms
        elapsed += (frame.tv_usec - lastFrame.tv_usec) / 1000.0;

        if (elapsed > 3.0){
            lastFrame = frame;

            struct timeval t1, t2;
            double elapsedTime;
            gettimeofday(&t1, NULL);

            exception = NULL;
            mono_runtime_invoke (update, objIface, NULL, &exception);
            if (exception)
                _printException(exception);

            gettimeofday(&t2, NULL);
            elapsedTime = (t2.tv_sec - t1.tv_sec) * 1000.0;      // sec to ms
            elapsedTime += (t2.tv_usec - t1.tv_usec) / 1000.0;
            char strElapsed[100];
            sprintf(strElapsed, "%lf", elapsedTime);
            _crow_notify_value_changed("update", strElapsed);
            _crow_notify_value_changed_d("fpsMin", elapsedTime);
        }
    }
}
void* _crow_thread (void *arg) {
    int retval;
    const char *crowDllPath = "Crow.dll";

    domainCrow = mono_jit_init (crowDllPath);
    mono_config_parse ("/etc/mono/config");
    assemblyCrow = mono_domain_assembly_open (domainCrow, crowDllPath);

    if (!assemblyCrow)
        return EXIT_FAILURE;

    imageCrow = mono_assembly_get_image (assemblyCrow);

    mono_add_internal_call ("Crow.Interface::crow_mono_update", _crow_mono_update);

    embed_cairo_init();

    clsIface = mono_class_from_name (imageCrow, "Crow", "Interface");
    if (!clsIface) {
        fprintf (stderr, "Can't find Interface in assembly %s\n", mono_image_get_filename (imageCrow));
        exit (1);
    }

    _search_iface_methods();

    objIface = mono_object_new (domainCrow, clsIface);

    mono_runtime_object_init (objIface);

    _main_loop();

    retval = mono_environment_exitcode_get ();

    mono_jit_cleanup (domainCrow);

    pthread_exit(retval);
}

void crow_init () {
    if(pthread_create(&crowThread, NULL, _crow_thread, NULL) == -1) {
        perror("pthread_create");
        exit (EXIT_FAILURE);
    }
}

void crow_lock_update_mutex(){
    pthread_mutex_lock(&crow_update_mutex);
}
void crow_release_update_mutex(){
    pthread_mutex_unlock(&crow_update_mutex);
}
