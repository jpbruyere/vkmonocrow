#include "crow.h"


static MonoDomain *domain;
static MonoAssembly *assembly;
static MonoImage *image;
static MonoClass *klass;
static MonoObject *obj;
static MonoMethod *loadiface = NULL, *resize = NULL, *update = NULL,
                    *mouseMove = NULL, *mouseUp = NULL, *mouseDown = NULL;
static MonoClassField *fldIsDirty = NULL, *fldDirtyRect = NULL, *fldDirtyBmp = NULL;


static pthread_t crowThread;
pthread_mutex_t crow_resize_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t crow_load_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t crow_dirty_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t crow_event_mutex = PTHREAD_MUTEX_INITIALIZER;

static uint8_t needResize = FALSE;
static uint8_t loadFlag = FALSE; //true when a file is triggered to be loaded
static Rectangle newBounds;
static char* fileName = NULL;

Rectangle dirtyRect = {};
uint8_t* dirtyBmp = NULL;
size_t dirtyBmpSize;

static CrowEvent * crow_evt_queue_start = NULL;
static CrowEvent * crow_evt_queue_end = NULL;

CrowEvent* crow_create_evt(uint32_t eventType, uint32_t data0, uint32_t data1){
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

void crow_resize (Rectangle bounds){
    pthread_mutex_lock(&crow_resize_mutex);
    newBounds = bounds;
    needResize = TRUE;
    pthread_mutex_unlock(&crow_resize_mutex);
}

void crow_load () {
    while(loadFlag)
        continue;
    pthread_mutex_lock(&crow_load_mutex);
    loadFlag = TRUE;
    pthread_mutex_unlock(&crow_load_mutex);
}

bool crow_get_bmp(){
    pthread_mutex_lock(&crow_dirty_mutex);
    if (dirtyBmp){
        //printf("Dirty:  (%d,%d,%d,%d)\n", dirtyRect.x, dirtyRect.y, dirtyRect.width, dirtyRect.height);
        return TRUE;
    }
    pthread_mutex_unlock(&crow_dirty_mutex);
    return FALSE;

}
void crow_release_dirty_mutex(){
    free(dirtyBmp);
    dirtyBmp=NULL;
    pthread_mutex_unlock(&crow_dirty_mutex);
}

void* crow_thread (void *arg) {
    printf ("Crow Interface Thread\n");

    int retval;
    const char *crowDllImage = "/mnt/devel/Crow/build/Debug/Crow.dll";

    domain = mono_jit_init (crowDllImage);
    mono_config_parse ("/etc/mono/config");
    assembly = mono_domain_assembly_open (domain, crowDllImage);

    image = mono_assembly_get_image (assembly);

    if (!assembly)
        return EXIT_FAILURE;

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
    }

    fldIsDirty = mono_class_get_field_from_name (klass, "IsDirty");
    fldDirtyRect = mono_class_get_field_from_name (klass, "DirtyRect");
    fldDirtyBmp = mono_class_get_field_from_name (klass, "dirtyBmp");

    obj = mono_object_new (domain, klass);
    mono_runtime_object_init (obj);

    while(1){
        MonoObject *exception = NULL;

        if (crow_evt_pending()){
            CrowEvent* evt = crow_evt_dequeue();
            if (evt->eventType == CROW_MOUSE_MOVE){
                void* args[2];
                args[0] = &evt->data0;
                args[1] = &evt->data1;
                mono_runtime_invoke (mouseMove, obj, args, &exception);
            }else if (evt->eventType == CROW_MOUSE_DOWN){
                void* args[1];
                args[0] = &evt->data0;
                mono_runtime_invoke (mouseDown, obj, args, &exception);
            }else if (evt->eventType == CROW_MOUSE_UP){
                void* args[1];
                args[0] = &evt->data0;
                mono_runtime_invoke (mouseUp, obj, args, &exception);
            }
            free(evt);
        }else if (needResize){
            pthread_mutex_lock(&crow_resize_mutex);
            void* args[1];
            args[0] = &newBounds;
            mono_runtime_invoke (resize, obj, args, &exception);
            needResize = FALSE;
            pthread_mutex_unlock(&crow_resize_mutex);
        }else if (loadFlag){
            pthread_mutex_lock(&crow_load_mutex);
            MonoString *str = mono_string_new (domain, "/mnt/devel/Crow/build/Debug/Interfaces/Divers/0.crow");
            mono_runtime_invoke (loadiface, obj, &str, &exception);
            loadFlag = FALSE;
            pthread_mutex_unlock(&crow_load_mutex);
        }else{
            //MonoBoolean* valIsDirty = NULL;
            uint8_t isDirty = 0;
            mono_field_get_value (obj, fldIsDirty, &isDirty);
             //= (uint8_t)*valIsDirty;
            if (isDirty){
                pthread_mutex_lock(&crow_dirty_mutex);

                mono_field_get_value (obj, fldDirtyRect, &dirtyRect);
                char* bmp = NULL;
                mono_field_get_value (obj, fldDirtyBmp, &bmp);
                dirtyBmpSize = (size_t)mono_array_length(bmp);

                if (dirtyBmp)
                    free(dirtyBmp);
                dirtyBmp = (uint8_t*)malloc(dirtyBmpSize);
                uint8_t* ptrBmp = (uint8_t*)mono_array_addr(bmp,uint8_t,0);

                memcpy(dirtyBmp,ptrBmp,dirtyBmpSize);

                isDirty = 0;
                mono_field_set_value(obj,fldIsDirty,&isDirty);
                pthread_mutex_unlock(&crow_dirty_mutex);
            }
            mono_runtime_invoke (update, obj, NULL, &exception);
        }
        if (exception)
            printException(exception);
    }

    retval = mono_environment_exitcode_get ();

    mono_jit_cleanup (domain);

    pthread_exit(retval);
}

void crow_init () {
    if(pthread_create(&crowThread, NULL, crow_thread, NULL) == -1) {
        perror("pthread_create");
        exit (EXIT_FAILURE);
    }

    //return retval;
}

