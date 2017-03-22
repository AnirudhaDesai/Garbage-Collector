#include "gcmalloc.hh"

template <class SourceHeap>
void * GCMalloc<SourceHeap>::malloc(size_t sz) {

  if (sz<=0)
      return NULL;
    Header *object;
    int classIndex = getSizeClass(sz); //Get the Class Index
    size_t roundUpSize = getSizeFromClass(classIndex); // Allocated Size 
    if (roundUpSize!=0)
    {
      heapLock.lock();
      if (freedObjects[classIndex]!=NULL) 
       {
          // Freed Objects of this class size available 
          object = freedObjects[classIndex];
          freedObjects[classIndex]=freedObjects[classIndex]->prevObject;
          if(freedObjects[classIndex]!=NULL)
            freedObjects[classIndex]->nextObject=NULL;
        }
       else
       {
          void * ptr = SourceHeap::malloc(roundUpSize+sizeof(Header));
          if(ptr==NULL)
          {
            // HeapRemaining is zero
            heapLock.unlock();
            return NULL;
          }
          object = (Header *)ptr;
       } 
     // object->requestedSize = sz;
     // object->allocatedSize = roundUpSize;
     // allocated+=roundUpSize;
     // requested+=sz;
     /* if(roundUpSize>=maxAllocated)
        maxAllocated=roundUpSize;
      if(sz>=maxRequested)
        maxRequested=sz; */
      /* Add the object to Allocated Objects List*/
      object->nextObject=NULL;
      object->prevObject=allocatedObjects;
      if(allocatedObjects!=NULL)
        allocatedObjects->nextObject=object;
      allocatedObjects=object;

      heapLock.unlock();
      return (object+1); 
  }
  return NULL;
  return nullptr; // FIX ME
}

template <class SourceHeap>
GCMalloc<SourceHeap>::GCMalloc() {
  }
  

template <class SourceHeap>
size_t GCMalloc<SourceHeap>::getSize(void * p) {
  return 0; // FIX ME
}

// number of bytes currently allocated  
template <class SourceHeap>
size_t GCMalloc<SourceHeap>::bytesAllocated() {
  return 0; // FIX ME
}


template <class SourceHeap>
void GCMalloc<SourceHeap>::walk(const std::function< void(Header *) >& f) {
  // FIX ME
}

template <class SourceHeap>
size_t GCMalloc<SourceHeap>::getSizeFromClass(int index) {
  return 0; // FIX ME
}


template <class SourceHeap>
int GCMalloc<SourceHeap>::getSizeClass(size_t sz) {
  return 0; // FIX ME
}
