/**
\file
\brief Charm++: Lists of possible Chares and Entry points.
*/
#ifndef _REGISTER_H
#define _REGISTER_H

#include "charm.h"
#include "cklists.h"

/** \addtogroup CkRegister */
/*@{*/

/**
Represents a single entry method or constructor.
EntryInfo's are always stored in the _entryTable, and can
be referred to across processors by their index into the
_entryTable--this is the entry point's index, often abbreviated
"epIdx".

Although this class is always created via a clean, well-defined
API (the CkRegister* routines in charm.h); access to these classes
is completely direct-- the ck.C routines just access, e.g.,
 _entryTable[epIdx]->chareIdx.
*/
class EntryInfo {
  public:
    /**
      A "call function" is how Charm++ actually invokes an 
      entry method on an object.  Call functions take two parameters:
        1.) The message to pass to the method.  This may be 
	    a regular message, a CkMarshallMsg for a 
	    parameter-marshalled method, or a "SysMsg" for a void method.
	    For migration constructors, the "message" might
	    even be NULL.
        2.) The object to invoke the method on.
      Call functions are always generated by the translator.
      
      A simple call function to invoke a method foo::bar(fooMsg *)
      might look like this:
        <pre>
        extern "C" void __call_foo_bar(void *msg,void *obj) {
           fooMsg *m=(fooMsg *)msg;
           foo *f=(foo *)obj;
           f->bar(m);
        }
        </pre>
      Call functions are even used to invoke constructors on new
      Chares.
    */
    CkCallFnPtr call;
    /// Our parameters' index into the _msgTable
    int msgIdx;
    /// Our chare's index into the _chareTable
    int chareIdx;

    /** 
      A "marshall unpack" function:
        1.) Pups method parameters out of the buffer passed in to it.
	2.) Calls a method on the object passed in.
	3.) Returns the number of bytes of the buffer consumed.
      It can be used for very efficient delivery of 
      a whole set of combined messages.
    */
    CkMarshallUnpackFn marshallUnpack;

#if CMK_CHARMDEBUG
    /** 
      A "message pup" function pups the message accepted by 
      this entry point.  This is *only* used to display the 
      message in the debugger, not for normal communication.
      
      This is registered with the entry point  
      (not the message) because many entry points take the same
      message type but store different data in it, like parameter
      marshalled messages.
    */
    CkMessagePupFn messagePup;
#endif

    /// Charm++ Tracing enabled for this ep (can change dynamically)
    bool traceEnabled;
    /// Method doesn't keep (and delete) message passed in to it.
    bool noKeep;
    /// true if this EP is charm internal functions
    bool inCharm;
    bool appWork;
#ifdef ADAPT_SCHED_MEM
   /// true if this EP is used to be rescheduled when adjusting memory usage
   bool isMemCritical;
#endif

    /// Human-readable name of entry method, including parameters.
    const char *name;

    EntryInfo(const char *n, CkCallFnPtr c, int m, int ci) : 
      call(c), msgIdx(m), chareIdx(ci),
      marshallUnpack(0)
#if CMK_CHARMDEBUG
      ,messagePup(0)
#endif
      ,traceEnabled(true), noKeep(false), inCharm(false), appWork(false)
      ,name(n)
    { }
};

/**
Represents one type of Message.
It is always stored in _msgTable.
*/
class MsgInfo {
  public:
    /**
      A message pack function converts the (possibly complex)
      message into a flat array of bytes.  This method is called
      whenever messages are sent or duplicated, so for speed 
      messages normally have a layout which allows this conversion
      to be done in-place, typically by just moving some pointers
      around in the message.
      
      There was once a time when the pack function could be NULL,
      meaning the message can be sent without packing, but today
      the pack function is always set.  Note: pack and unpack have
      nothing to do with the PUP framework.
     */
    CkPackFnPtr pack;
    /**
     A message unpack function converts a flat array of bytes into
     a living message.  It's just the opposite of pack.
     */
    CkUnpackFnPtr unpack;
    /**
     A message deallocation function deletes the message with the
     appropriate actions.
     */
    CkDeallocFnPtr dealloc;
    /**
     This message body's allocation size.  This does *not* include
     any dynamically allocated portion of the message, so is a lower
     bound on the message size.
     */
    size_t size;

    /// Human-readable name of message, like "CkMarshallMsg".
    const char *name;

    MsgInfo(const char *n,CkPackFnPtr p,CkUnpackFnPtr u,CkDeallocFnPtr d,int s):
      pack(p), unpack(u), dealloc(d), size(s), name(n)
    {}
};

/**
Represents a class of Chares (or array or group elements).  
It is always stored in the _chareTable.
*/
class ChareInfo {
  public:
    /// Human-readable name of the chare class, like "MyFoo".
    const char *name;
    /// Size, in bytes, of the body of the chare.
    size_t size;
    
    /// Constructor epIdx: default constructor and migration constructor (or -1 if none).
    int defCtor,migCtor; 
    /// Number of base classes:
    int numbases;
    /// chareIdx of each base class.
    int bases[16];
    
    /// chare type
    ChareType  chareType;
    int mainChareIdx;

    /// Number of dimensions for chare arrays (-1 if not an array, 0 is invalid)
    int ndims;

    /// For groups -- true if the group is Irreducible
    bool isIrr;

    /// true if this EP is charm internal functions
    bool inCharm;

    ChareInfo(const char *n, size_t s, ChareType t) : name(n), size(s) {
      defCtor=migCtor=-1;
      isIrr = numbases = 0;
      chareType = t;
      inCharm = false;
      mainChareIdx = -1;
      ndims = -1;
    }
    void setDefaultCtor(int idx) { defCtor = idx; }
    int getDefaultCtor(void) { return defCtor; }
    void setMigCtor(int idx) { migCtor = idx; }
    int getMigCtor(void) { return migCtor; }
    void addBase(int idx) {
      CkAssert(numbases+1 <= 16); // 'bases' is declared as an array of size 16 above,
                                  // but we could potentially make it a a std::vector
      bases[numbases++] = idx;
    }
    void setInCharm() { inCharm = true; }
    bool isInCharm() { return inCharm; }
    void setMainChareType(int idx) { mainChareIdx = idx; }
    int mainChareType() { return mainChareIdx; }
};

/// Describes a mainchare's constructor.  These are all executed at startup.
class MainInfo {
  public:
    void* obj; // real type is Chare*
    const char *name;
    int chareIdx;
    int entryIdx;
    int entryMigCtor;
    MainInfo(int c, int e) : name("main"), chareIdx(c), entryIdx(e) {}
    inline void* getObj(void) { return obj; }
    inline void setObj(void *_obj) { obj=_obj; }
};

/// Describes a readonly global variable.
class ReadonlyInfo {
  public:
    /// Size in bytes of basic value.
    size_t size;
    /// Address of basic value.
    void *ptr;
    /// Pup routine for value, or NULL if no pup available.
    CkPupReadonlyFnPtr pup;
    /// Human-readable string name of variable (e.g., "nElements") and type (e.g., "int").
    const char *name,*type;
    
    /// Pup this global variable.
    void pupData(PUP::er &p) {
      if (pup!=NULL)
        (pup)((void *)&p);
      else
        p((char *)ptr,size);
    }
    ReadonlyInfo(const char *n,const char *t,
	 size_t s, void *p,CkPupReadonlyFnPtr pf)
	: size(s), ptr(p), pup(pf), name(n), type(t) {}
};

/**
 Describes a readonly message.  Readonly messages were
 once the only way to get a truly variable-sized readonly,
 but now that readonlies are pup'd they are almost totally useless.
*/
class ReadonlyMsgInfo {
  public:
    const char *name, *type;
    void **pMsg;
    ReadonlyMsgInfo(const char *n, const char *t,
	void **p) : name(n), type(t), pMsg(p) {}
};

/**
 This class stores registered entities, like EntryInfo's,
 in a linear list indexed by index ("idx").
*/
template <class T>
class CkRegisteredInfo {
	CkVec<T *> vec;
	
	void outOfBounds(int idx) {
		const char *exampleName="";
		if (vec.size()>0) exampleName=vec[0]->name;
		CkPrintf("register.h> CkRegisteredInfo<%d,%s> called with invalid index "
			"%d (should be less than %d)\n", sizeof(T),exampleName,
			idx, vec.size());
		CkAbort("Registered idx is out of bounds-- is message or memory corrupted?");
	}
public:
	/**
	Subtle: we *don't* want to call vec's constructor,
	because the order in which constructors for global
	variables get called is undefined.
	Hence we rely on the implicit zero-initialization
	that all globals get.
	*/
	CkRegisteredInfo() :vec(CkSkipInitialization()) {}
        ~CkRegisteredInfo() {
        	for (size_t i=0; i<vec.size(); i++) if (vec[i]) delete vec[i];
  	} 

	/// Add a heap-allocated registration record,
	///  returning the index used.
	int add(T *t) {
#if CMK_ERROR_CHECKING
		/* Make sure registrations only happen from rank 0: */
		if (CkMyRank()!=0)
			CkAbort("Can only do registrations from rank 0 processors");
#endif
		vec.push_back(t);
		return vec.size()-1;
	}
	
	size_t size(void) {return vec.size();}
	
	/// Return the registered data at this index.
	T *operator[](size_t idx) {
#if CMK_ERROR_CHECKING
		/* Bounds-check the index: */
		if (idx>=vec.size()) outOfBounds(idx);
#endif
		return vec[idx];
	}
};

/// These tables are shared between all processors on a node.
extern CkRegisteredInfo<EntryInfo> _entryTable;
extern CkRegisteredInfo<MsgInfo> _msgTable;
extern CkRegisteredInfo<ChareInfo> _chareTable;
extern CkRegisteredInfo<MainInfo> _mainTable;
extern CkRegisteredInfo<ReadonlyInfo> _readonlyTable;
extern CkRegisteredInfo<ReadonlyMsgInfo> _readonlyMsgs;

extern void _registerInit(void);
extern void _registerDone(void);

extern int CkGetChareIdx(const char *name);

/*@}*/
#endif
