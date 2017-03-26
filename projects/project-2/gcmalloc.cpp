#include "gcmalloc.hh"

template <class SourceHeap>
void * GCMalloc<SourceHeap>::malloc(size_t sz) {

  if (sz<=0)
      return NULL;


    Header *object;
    int classIndex = getSizeClass(sz); //Get the Class Index
    size_t roundUpSize = getSizeFromClass(classIndex); // Allocated Size 

    /* Check if GC needs to be triggered */
    if (triggerGC(roundUpSize) && !(inGC))
    {
      
      gc();
      
    } 

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
          //printf("Malloc on %p\n",ptr );
          if(ptr==NULL)
          {
            // HeapRemaining is zero
            heapLock.unlock();
            return NULL;
          }
          object = (Header *)ptr;
          endHeap = (void *)((char *)(SourceHeap::getStart())+ SourceHeap::getSize() - SourceHeap::getRemaining());
       } 
     
       object->setCookie();
       object->setAllocatedSize(roundUpSize);
       allocated+=roundUpSize;
       objectsAllocated++;
       bytesAllocatedSinceLastGC+= roundUpSize;

     
      /* Add the object to Allocated Objects List*/
      object->nextObject=NULL;
      object->prevObject=allocatedObjects;
      if(allocatedObjects!=NULL)
        allocatedObjects->nextObject=object;
      allocatedObjects=object;

      heapLock.unlock();
      return (object+1); 
  }
  //return NULL;
  return nullptr; // FIX ME
}

template <class SourceHeap>
void  GCMalloc<SourceHeap>::privateFree(void *ptr){

  if (ptr != NULL)
  {   
    Header *object = (Header *)(ptr) - 1; // Redirect the pointer to header
    int classIndex =  getSizeClass(object->getAllocatedSize());
    heapLock.lock();
    if (object->prevObject!=NULL)
      object->prevObject->nextObject = object->nextObject;
    if (object->nextObject!=NULL)
      object->nextObject->prevObject = object->prevObject;
    object->nextObject = NULL;
    if (object==allocatedObjects) //Last object in allocatedObjects to be freed
      allocatedObjects=allocatedObjects->prevObject; 
    object->prevObject=freedObjects[classIndex];
    if (freedObjects[classIndex]!=NULL) 
      freedObjects[classIndex]->nextObject=object;
    freedObjects[classIndex]=object; 
    
    allocated-=object->getAllocatedSize(); //  Decrement Allocated size 
    heapLock.unlock();
  } 
  return;
  }

template <class SourceHeap>
size_t GCMalloc<SourceHeap>::getSize(void * p) {
  if (p != NULL)
  {
    Header *object = (Header *) p - 1;
    return object->getAllocatedSize();
  }
  return 0;
}

// number of bytes currently allocated  
template <class SourceHeap>
size_t GCMalloc<SourceHeap>::bytesAllocated() {
  return allocated; // FIX ME
}

template <class SourceHeap>
void GCMalloc<SourceHeap>::walk(const std::function< void(Header *) >& f) {
  // FIX ME
}

template <class SourceHeap>
size_t GCMalloc<SourceHeap>::getSizeFromClass(int index) {
   long int switchIndex = Threshold/Base;  
  if (index < switchIndex)
  {
    return (size_t)(Base*(index+1));
  }
  else if(index<=(switchIndex+14) && index>=switchIndex)
  {
    return (size_t)(pow(2,(index-switchIndex)+15));
  }
  return 0; 
}


template <class SourceHeap>
int constexpr GCMalloc<SourceHeap>::getSizeClass(size_t sz) {
  if (sz<=Threshold)
  {
    return (int) (ceil((sz/(float)Base)) - 1);
  }  
  else 
  {
   return (int)(ceil(log2(sz))+(Threshold/Base)-15);
  }
}

template <class SourceHeap>
GCMalloc<SourceHeap>::GCMalloc() 
  : bytesAllocatedSinceLastGC(0),
    bytesReclaimedLastGC(0),
    startHeap(SourceHeap::getStart()),
    endHeap((void *)((char *)(SourceHeap::getStart())+ SourceHeap::getSize() - SourceHeap::getRemaining())),
    objectsAllocated(0),
    allocated(0),
    allocatedObjects(nullptr),
    initialized(true),
    nextGC(1024*10)
     {

    for (auto& f : freedObjects) {
      f = nullptr;
    }
  }

template <class SourceHeap>
void GCMalloc<SourceHeap>::scan(void * start, void * end) {
    while(start < end)
    {
      void ** temp = (void**)start;
     // printf("Recursing into markReachable\n");
      markReachable((void*)*temp);
      start = (void*)((uintptr_t)start+1);
      //printf("scan called on %p \n", start );
    }
  } 

template <class SourceHeap>
bool GCMalloc<SourceHeap>::triggerGC(size_t szRequested) {
  if((bytesAllocatedSinceLastGC + szRequested)> SourceHeap::getRemaining())
    return true;
 /* if (freedObjects[getSizeClass(szRequested)]==NULL)
    return true; */
  if ((bytesAllocatedSinceLastGC + szRequested) > nextGC)
      return true;
  return false;
}

template <class SourceHeap>
void GCMalloc<SourceHeap>::gc() {

  inGC = true;
  bytesReclaimedLastGC=0;
  mark();
  sweep();
  inGC = false;
  bytesAllocatedSinceLastGC = 0;
  

  }

template <class SourceHeap>
void GCMalloc<SourceHeap>::mark() {
  //printf("Walking Stack \n");
  sp.walkStack([this] (void *p) {markReachable(p);});
  //printf("Walking Globals\n");
  sp.walkGlobals([this] (void *p) {markReachable(p);});
  //printf("Walking Registers\n");
  sp.walkRegisters([this] (void *p) {markReachable(p);});

  }
template <class SourceHeap>
void GCMalloc<SourceHeap>::markReachable(void * ptr) {
    if (isPointer(ptr))
    {
      /* Find Header using Validate Cookie */
      Header * object = (Header *) ptr;
      while(!object->validateCookie())
      { 
       // printf(" Object without header %p\n", object);
        object = (Header *)((uintptr_t)object-1);

      }
    //  printf(" Object with Header %p \n", object );
      if(!object->isMarked())
      {
        printf("Object marked\n");
        object->mark();
        void * ScanStart = (void*)(object + 1);
        void * ScanEnd =  (void*)(((char *)(object + 1) + object->getAllocatedSize()));
        scan(ScanStart,ScanEnd);
      }
    }

  }

template <class SourceHeap>
void GCMalloc<SourceHeap>::sweep() {
    Header * beginSweep = (Header *)startHeap;
      printf("Entered Sweep\n");
    while(beginSweep<endHeap)
    {
      printf("Entered while in sweep\n");
      if(!(beginSweep->isMarked()))
      {
        
        privateFree(beginSweep+1);
        bytesReclaimedLastGC+= beginSweep->getAllocatedSize();  
        //printf("privateFree called on %p \n", beginSweep);
      }
      else
      {
        beginSweep->clear();
      }
      beginSweep = (Header *)((char *)(beginSweep+1) + beginSweep->getAllocatedSize());
    
    }

  }

template <class SourceHeap>
bool GCMalloc<SourceHeap>::isPointer(void * p) {
    if(p>=startHeap && p<endHeap)
      return true;
    return false;
  }
