#include "alloc.h"
#include "carray.h"
#include "buffer.h"

/**
 * If CARRAY_GC_DEBUG env is True, CArray Garbage Collector
 * will print debug messages when destructing objects.
 */
static int
CArrayGC_ISDEBUGON()
{
    if (getenv("CARRAY_GC_DEBUG") == NULL) {
        return 0;
    }
    if (!strcmp(getenv("CARRAY_GC_DEBUG"), "0")) {
        return 0;
    }
    return 1;
}

void
CArrayDescriptor_FREE(CArrayDescriptor * descr)
{
    if (descr->refcount <= 0) {
        efree(descr->f);
        efree(descr);
        descr = NULL;
    }
}

/**
 * @return
 */
void
CArray_INCREF(CArray *target)
{
    target->refcount++;
}

/**
 * @return
 */
void
CArray_XDECREF(CArray *target)
{
    target->refcount--;
}

/**
 * @return
 */
void
CArray_DECREF(CArray *target)
{
    target->refcount--;
    CArray_Free(target);
}


/**
 * Alocates CArray Data Buffer based on numElements and elsize from
 * CArray descriptor.
 **/
void
CArray_Data_alloc(CArray * ca)
{
    ca->data = emalloc((ca->descriptor->numElements * ca->descriptor->elsize));
}

/**
 * @param size
 * @return
 */
void *
carray_data_alloc_zeros(int num_elements, int size_element, char type)
{
    return (void*)ecalloc(num_elements, size_element);
}

/**
 * @param size
 * @return
 */
void *
carray_data_alloc(int num_elements, int size_element)
{
    return (void*)emalloc(num_elements * size_element);
}

/**
 * @param descriptor
 */
void
CArrayDescriptor_INCREF(CArrayDescriptor * descriptor)
{
    descriptor->refcount++;
}

/**
 * @param descriptor
 */
void
CArrayDescriptor_DECREF(CArrayDescriptor * descriptor)
{
    if (descriptor != NULL) {
        descriptor->refcount--;
    }
}

void
CArray_Free(CArray * self)
{
    if(self->refcount <= 0) {
        efree(self->dimensions);
        efree(self->strides);
        efree(self->data);
        efree(self);
    } else {
        efree(self->dimensions);
        efree(self->strides);
        efree(self);
    }
}

/**
 * Free CArrays owning data buffer
 */
void
_free_data_owner(MemoryPointer * ptr)
{
    CArray * array = CArray_FromMemoryPointer(ptr);
    CArrayDescriptor_DECREF(array->descriptor);

    if(array->descriptor->refcount < 0) {
        if (CArrayGC_ISDEBUGON()) {
          php_printf("\n[CARRAY_GC_DEBUG][DESCR] Freeing Descriptor from CArray ID %d", ptr->uuid);
        }
        CArrayDescriptor_FREE(CArray_DESCR(array));
    }

    CArray_XDECREF(array);
    if (CArrayGC_ISDEBUGON()) {
      php_printf("\n[CARRAY_GC_DEBUG] Freeing Dimensions and Strides from CArray ID %d", ptr->uuid);
    }
    efree(array->dimensions);
    efree(array->strides);
    if(array->refcount < 0) {
        if (CArrayGC_ISDEBUGON()) {
          php_printf("\n[CARRAY_GC_DEBUG] Freeing DATA from CArray ID %d", ptr->uuid);
        }
        efree(array->data);
        buffer_remove(ptr);
    }
}

/**
 * Free CArrays that refers others CArrays
 */
void
_free_data_ref(MemoryPointer * ptr)
{
    MemoryPointer tmp;

    CArray * array = CArray_FromMemoryPointer(ptr);

    CArray_XDECREF(array);
    CArray_XDECREF(array->base);
    CArrayDescriptor_DECREF(array->descriptor);
    if(array->descriptor->refcount < 0) {
        if (CArrayGC_ISDEBUGON()) {
          php_printf("\n[CARRAY_GC_DEBUG][DESCR][VIEW] Freeing Descriptor from CArray ID %d", ptr->uuid);
        }
        CArrayDescriptor_FREE(CArray_DESCR(array));
    }
    efree(array->dimensions);
    efree(array->strides);

    if(array->refcount < 0 && array->base->refcount < 0) {
        if (CArrayGC_ISDEBUGON()) {
          php_printf("\n[CARRAY_GC_DEBUG][VIEW] Freeing CArray ID %d", ptr->uuid);
        }
        if(CArray_CHKFLAGS(array->base, CARRAY_ARRAY_OWNDATA)) {
            if (CArrayGC_ISDEBUGON()) {
              php_printf("\n[CARRAY_GC_DEBUG][VIEW] Freeing DATA from CArray ID %d", ptr->uuid);
            }
            efree(array->data);
        }
        tmp.uuid = array->base->uuid;
        buffer_remove(&tmp);
    }
    if(array->refcount < 0){
        if (CArrayGC_ISDEBUGON()) {
          php_printf("\n[CARRAY_GC_DEBUG][VIEW] Freeing CArray ID %d", ptr->uuid);
        }
        buffer_remove(ptr);
    }
}

/**
 * Free CArray using MemoryPointer
 **/
void
CArray_Alloc_FreeFromMemoryPointer(MemoryPointer * ptr)
{
    CArray * array = CArray_FromMemoryPointer(ptr);
    if(CArray_CHKFLAGS(array, CARRAY_ARRAY_OWNDATA)){
        _free_data_owner(ptr);
    } else {
        _free_data_ref(ptr);
    }
    return;
}

CArray *
CArray_Alloc(CArrayDescriptor *descr, int nd, int* dims,
             int is_fortran, void *interfaceData)
{
    CArray * target = emalloc(sizeof(CArray));
    return CArray_NewFromDescr(target, descr, nd, dims, NULL, NULL,
                              ( is_fortran ? CARRAY_ARRAY_F_CONTIGUOUS : 0),
                              interfaceData);
}
