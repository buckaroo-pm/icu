/*
*******************************************************************************
*
*   Copyright (C) 2009, International Business Machines
*   Corporation and others.  All Rights Reserved.
*
*******************************************************************************
*   file name:  normalizer2.cpp
*   encoding:   US-ASCII
*   tab size:   8 (not used)
*   indentation:4
*
*   created on: 2009nov22
*   created by: Markus W. Scherer
*/

#include "unicode/utypes.h"

#if !UCONFIG_NO_NORMALIZATION

#include "unicode/localpointer.h"
#include "unicode/normalizer2.h"
#include "unicode/unistr.h"
#include "unicode/unorm.h"
#include "cpputils.h"
#include "cstring.h"
#include "mutex.h"
#include "normalizer2impl.h"
#include "ucln_cmn.h"

U_NAMESPACE_BEGIN

// Public API dispatch via Normalizer2 subclasses -------------------------- ***

// Normalizer2 implementation for the old UNORM_NONE.
class NoopNormalizer2 : public Normalizer2 {
    virtual UnicodeString &
    normalize(const UnicodeString &src,
              UnicodeString &dest,
              UErrorCode &errorCode) const {
        if(U_SUCCESS(errorCode)) {
            if(&dest!=&src) {
                dest=src;
            } else {
                errorCode=U_ILLEGAL_ARGUMENT_ERROR;
            }
        }
        return dest;
    }
    virtual UnicodeString &
    normalizeSecondAndAppend(UnicodeString &first,
                             const UnicodeString &second,
                             UErrorCode &errorCode) const {
        if(U_SUCCESS(errorCode)) {
            first.append(second);
        }
        return first;
    }
    virtual UnicodeString &
    append(UnicodeString &first,
           const UnicodeString &second,
           UErrorCode &errorCode) const {
        if(U_SUCCESS(errorCode)) {
            if(&first!=&second) {
                first.append(second);
            } else {
                errorCode=U_ILLEGAL_ARGUMENT_ERROR;
            }
        }
        return first;
    }
    virtual UBool
    isNormalized(const UnicodeString &s, UErrorCode &errorCode) const {
        return TRUE;
    }
    virtual UNormalizationCheckResult
    quickCheck(const UnicodeString &s, UErrorCode &errorCode) const {
        return UNORM_YES;
    }
    virtual int32_t
    spanQuickCheckYes(const UnicodeString &s, UErrorCode &errorCode) const {
        return s.length();
    }
    virtual UBool hasBoundaryBefore(UChar32 c) const { return TRUE; }
    virtual UBool hasBoundaryAfter(UChar32 c) const { return TRUE; }
    virtual UBool isInert(UChar32 c) const { return TRUE; }

    static UClassID U_EXPORT2 getStaticClassID();
    virtual UClassID getDynamicClassID() const;
};

UOBJECT_DEFINE_RTTI_IMPLEMENTATION(NoopNormalizer2)

// Intermediate class:
// Has Normalizer2Impl and does boilerplate argument checking and setup.
class Normalizer2WithImpl : public Normalizer2 {
public:
    Normalizer2WithImpl(const Normalizer2Impl &ni) : impl(ni) {}

    // normalize
    virtual UnicodeString &
    normalize(const UnicodeString &src,
              UnicodeString &dest,
              UErrorCode &errorCode) const {
        uprv_checkCanGetBuffer(src, errorCode);
        if(U_FAILURE(errorCode)) {
            dest.setToBogus();
            return dest;
        }
        if(&dest==&src) {
            errorCode=U_ILLEGAL_ARGUMENT_ERROR;
            return dest;
        }
        dest.remove();
        ReorderingBuffer buffer(impl, dest);
        if(buffer.init(errorCode)) {
            const UChar *sArray=src.getBuffer();
            normalize(sArray, sArray+src.length(), buffer, errorCode);
        }
        return dest;
    }
    virtual void
    normalize(const UChar *src, const UChar *limit,
              ReorderingBuffer &buffer, UErrorCode &errorCode) const = 0;

    // normalize and append
    virtual UnicodeString &
    normalizeSecondAndAppend(UnicodeString &first,
                             const UnicodeString &second,
                             UErrorCode &errorCode) const {
        return normalizeSecondAndAppend(first, second, TRUE, errorCode);
    }
    virtual UnicodeString &
    append(UnicodeString &first,
           const UnicodeString &second,
           UErrorCode &errorCode) const {
        return normalizeSecondAndAppend(first, second, FALSE, errorCode);
    }
    UnicodeString &
    normalizeSecondAndAppend(UnicodeString &first,
                             const UnicodeString &second,
                             UBool doNormalize,
                             UErrorCode &errorCode) const {
        uprv_checkCanGetBuffer(first, errorCode);
        uprv_checkCanGetBuffer(second, errorCode);
        if(U_FAILURE(errorCode)) {
            return first;
        }
        if(&first==&second) {
            errorCode=U_ILLEGAL_ARGUMENT_ERROR;
            return first;
        }
        ReorderingBuffer buffer(impl, first);
        if(buffer.init(errorCode)) {
            const UChar *secondArray=second.getBuffer();
            normalizeAndAppend(secondArray, secondArray+second.length(), doNormalize,
                               buffer, errorCode);
        }
        return first;
    }
    virtual void
    normalizeAndAppend(const UChar *src, const UChar *limit, UBool doNormalize,
                       ReorderingBuffer &buffer, UErrorCode &errorCode) const = 0;

    // quick checks
    virtual UBool
    isNormalized(const UnicodeString &s, UErrorCode &errorCode) const {
        uprv_checkCanGetBuffer(s, errorCode);
        if(U_FAILURE(errorCode)) {
            return FALSE;
        }
        const UChar *sArray=s.getBuffer();
        const UChar *sLimit=sArray+s.length();
        return sLimit==spanQuickCheckYes(sArray, sLimit, errorCode);
    }
    virtual UNormalizationCheckResult
    quickCheck(const UnicodeString &s, UErrorCode &errorCode) const {
        return Normalizer2WithImpl::isNormalized(s, errorCode) ? UNORM_YES : UNORM_NO;
    }
    virtual int32_t
    spanQuickCheckYes(const UnicodeString &s, UErrorCode &errorCode) const {
        uprv_checkCanGetBuffer(s, errorCode);
        if(U_FAILURE(errorCode)) {
            return 0;
        }
        const UChar *sArray=s.getBuffer();
        return (int32_t)(spanQuickCheckYes(sArray, sArray+s.length(), errorCode)-sArray);
    }
    virtual const UChar *
    spanQuickCheckYes(const UChar *src, const UChar *limit, UErrorCode &errorCode) const = 0;

    static UClassID U_EXPORT2 getStaticClassID();
    virtual UClassID getDynamicClassID() const;

    const Normalizer2Impl &impl;
};

UOBJECT_DEFINE_RTTI_IMPLEMENTATION(Normalizer2WithImpl)

class DecomposeNormalizer2 : public Normalizer2WithImpl {
public:
    DecomposeNormalizer2(const Normalizer2Impl &ni) : Normalizer2WithImpl(ni) {}

    virtual void
    normalize(const UChar *src, const UChar *limit,
              ReorderingBuffer &buffer, UErrorCode &errorCode) const {
        impl.decompose(src, limit, &buffer, errorCode);
    }
    virtual void
    normalizeAndAppend(const UChar *src, const UChar *limit, UBool doNormalize,
                       ReorderingBuffer &buffer, UErrorCode &errorCode) const {
        impl.decomposeAndAppend(src, limit, doNormalize, buffer, errorCode);
    }
    virtual const UChar *
    spanQuickCheckYes(const UChar *src, const UChar *limit, UErrorCode &errorCode) const {
        return impl.decompose(src, limit, NULL, errorCode);
    }
    virtual UBool hasBoundaryBefore(UChar32 c) const { return impl.hasDecompBoundary(c, TRUE); }
    virtual UBool hasBoundaryAfter(UChar32 c) const { return impl.hasDecompBoundary(c, FALSE); }
    virtual UBool isInert(UChar32 c) const { return impl.isDecompInert(c); }
};

class ComposeNormalizer2 : public Normalizer2WithImpl {
public:
    ComposeNormalizer2(const Normalizer2Impl &ni, UBool fcc) :
        Normalizer2WithImpl(ni), onlyContiguous(fcc) {}

    virtual void
    normalize(const UChar *src, const UChar *limit,
              ReorderingBuffer &buffer, UErrorCode &errorCode) const {
        impl.compose(src, limit, onlyContiguous, NULL, &buffer, errorCode);
    }
    virtual void
    normalizeAndAppend(const UChar *src, const UChar *limit, UBool doNormalize,
                       ReorderingBuffer &buffer, UErrorCode &errorCode) const {
        impl.composeAndAppend(src, limit, doNormalize, onlyContiguous, buffer, errorCode);
    }

    virtual UBool
    isNormalized(const UnicodeString &s, UErrorCode &errorCode) const {
        uprv_checkCanGetBuffer(s, errorCode);
        if(U_FAILURE(errorCode)) {
            return UNORM_MAYBE;
        }
        UnicodeString temp;
        ReorderingBuffer buffer(impl, temp);
        if(!buffer.init(errorCode)) {
            return UNORM_MAYBE;
        }
        UNormalizationCheckResult qcResult=UNORM_YES;
        const UChar *sArray=s.getBuffer();
        impl.compose(sArray, sArray+s.length(), onlyContiguous, &qcResult, &buffer, errorCode);
        return qcResult==UNORM_YES;
    }
    virtual UNormalizationCheckResult
    quickCheck(const UnicodeString &s, UErrorCode &errorCode) const {
        uprv_checkCanGetBuffer(s, errorCode);
        if(U_FAILURE(errorCode)) {
            return UNORM_MAYBE;
        }
        UNormalizationCheckResult qcResult=UNORM_YES;
        const UChar *sArray=s.getBuffer();
        impl.compose(sArray, sArray+s.length(), onlyContiguous, &qcResult, NULL, errorCode);
        return qcResult;
    }
    virtual const UChar *
    spanQuickCheckYes(const UChar *src, const UChar *limit, UErrorCode &errorCode) const {
        UNormalizationCheckResult qcResult=UNORM_YES;
        return impl.compose(src, limit, onlyContiguous, &qcResult, NULL, errorCode);
    }
    virtual UBool hasBoundaryBefore(UChar32 c) const { return impl.hasCompBoundaryBefore(c); }
    virtual UBool hasBoundaryAfter(UChar32 c) const { return impl.hasCompBoundaryAfter(c, onlyContiguous, FALSE); }
    virtual UBool isInert(UChar32 c) const { return impl.hasCompBoundaryAfter(c, onlyContiguous, TRUE); }
private:
    UBool onlyContiguous;
};

class FCDNormalizer2 : public Normalizer2WithImpl {
public:
    FCDNormalizer2(const Normalizer2Impl &ni) : Normalizer2WithImpl(ni) {}

    virtual void
    normalize(const UChar *src, const UChar *limit,
              ReorderingBuffer &buffer, UErrorCode &errorCode) const {
        impl.makeFCD(src, limit, &buffer, errorCode);
    }
    virtual void
    normalizeAndAppend(const UChar *src, const UChar *limit, UBool doNormalize,
                       ReorderingBuffer &buffer, UErrorCode &errorCode) const {
        impl.makeFCDAndAppend(src, limit, doNormalize, buffer, errorCode);
    }
    virtual const UChar *
    spanQuickCheckYes(const UChar *src, const UChar *limit, UErrorCode &errorCode) const {
        return impl.makeFCD(src, limit, NULL, errorCode);
    }
    virtual UBool hasBoundaryBefore(UChar32 c) const { return impl.hasFCDBoundaryBefore(c); }
    virtual UBool hasBoundaryAfter(UChar32 c) const { return impl.hasFCDBoundaryAfter(c); }
    virtual UBool isInert(UChar32 c) const { return impl.isFCDInert(c); }
};

// instance cache ---------------------------------------------------------- ***

struct Norm2AllModes : public UMemory {
    static Norm2AllModes *createInstance(const char *packageName,
                                         const char *name,
                                         UErrorCode &errorCode);
    Norm2AllModes() : comp(impl, FALSE), decomp(impl), fcd(impl), fcc(impl, TRUE) {}

    Normalizer2Impl impl;
    ComposeNormalizer2 comp;
    DecomposeNormalizer2 decomp;
    FCDNormalizer2 fcd;
    ComposeNormalizer2 fcc;
};

Norm2AllModes *
Norm2AllModes::createInstance(const char *packageName,
                              const char *name,
                              UErrorCode &errorCode) {
    if(U_FAILURE(errorCode)) {
        return NULL;
    }
    LocalPointer<Norm2AllModes> allModes(new Norm2AllModes);
    if(allModes.isNull()) {
        errorCode=U_MEMORY_ALLOCATION_ERROR;
        return NULL;
    }
    allModes->impl.load(packageName, name, errorCode);
    return U_SUCCESS(errorCode) ? allModes.orphan() : NULL;
}

U_CDECL_BEGIN
static UBool U_CALLCONV uprv_normalizer2_cleanup();
U_CDECL_END

class Norm2AllModesSingleton : public TriStateSingletonWrapper<Norm2AllModes> {
public:
    Norm2AllModesSingleton(TriStateSingleton &s, const char *n) :
        TriStateSingletonWrapper<Norm2AllModes>(s), name(n) {}
    Norm2AllModes *getInstance(UErrorCode &errorCode) {
        return TriStateSingletonWrapper<Norm2AllModes>::getInstance(createInstance, name, errorCode);
    }
private:
    static void *createInstance(const void *context, UErrorCode &errorCode) {
        ucln_common_registerCleanup(UCLN_COMMON_NORMALIZER2, uprv_normalizer2_cleanup);
        return Norm2AllModes::createInstance(NULL, (const char *)context, errorCode);
    }

    const char *name;
};

STATIC_TRI_STATE_SINGLETON(nfcSingleton);
STATIC_TRI_STATE_SINGLETON(nfkcSingleton);
STATIC_TRI_STATE_SINGLETON(nfkc_cfSingleton);

class Norm2Singleton : public SimpleSingletonWrapper<Normalizer2> {
public:
    Norm2Singleton(SimpleSingleton &s) : SimpleSingletonWrapper<Normalizer2>(s) {}
    Normalizer2 *getInstance(UErrorCode &errorCode) {
        return SimpleSingletonWrapper<Normalizer2>::getInstance(createInstance, NULL, errorCode);
    }
private:
    static void *createInstance(const void *context, UErrorCode &errorCode) {
        Normalizer2 *noop=new NoopNormalizer2;
        if(noop==NULL) {
            errorCode=U_MEMORY_ALLOCATION_ERROR;
        }
        ucln_common_registerCleanup(UCLN_COMMON_NORMALIZER2, uprv_normalizer2_cleanup);
        return noop;
    }
};

STATIC_SIMPLE_SINGLETON(noopSingleton);

U_CDECL_BEGIN

static UBool U_CALLCONV uprv_normalizer2_cleanup() {
    Norm2AllModesSingleton(nfcSingleton, NULL).deleteInstance();
    Norm2AllModesSingleton(nfkcSingleton, NULL).deleteInstance();
    Norm2AllModesSingleton(nfkc_cfSingleton, NULL).deleteInstance();
    Norm2Singleton(noopSingleton).deleteInstance();
    return TRUE;
}

U_CDECL_END

const Normalizer2 *Normalizer2Factory::getNFCInstance(UErrorCode &errorCode) {
    Norm2AllModes *allModes=Norm2AllModesSingleton(nfcSingleton, "nfc").getInstance(errorCode);
    return allModes!=NULL ? &allModes->comp : NULL;
}

const Normalizer2 *Normalizer2Factory::getNFDInstance(UErrorCode &errorCode) {
    Norm2AllModes *allModes=Norm2AllModesSingleton(nfcSingleton, "nfc").getInstance(errorCode);
    return allModes!=NULL ? &allModes->decomp : NULL;
}

const Normalizer2 *Normalizer2Factory::getFCDInstance(UErrorCode &errorCode) {
    Norm2AllModes *allModes=Norm2AllModesSingleton(nfcSingleton, "nfc").getInstance(errorCode);
    if(allModes!=NULL) {
        allModes->impl.getFCDTrie(errorCode);
        return &allModes->fcd;
    } else {
        return NULL;
    }
}

const Normalizer2 *Normalizer2Factory::getFCCInstance(UErrorCode &errorCode) {
    Norm2AllModes *allModes=Norm2AllModesSingleton(nfcSingleton, "nfc").getInstance(errorCode);
    return allModes!=NULL ? &allModes->fcc : NULL;
}

const Normalizer2 *Normalizer2Factory::getNFKCInstance(UErrorCode &errorCode) {
    Norm2AllModes *allModes=
        Norm2AllModesSingleton(nfkcSingleton, "nfkc").getInstance(errorCode);
    return allModes!=NULL ? &allModes->comp : NULL;
}

const Normalizer2 *Normalizer2Factory::getNFKDInstance(UErrorCode &errorCode) {
    Norm2AllModes *allModes=
        Norm2AllModesSingleton(nfkcSingleton, "nfkc").getInstance(errorCode);
    return allModes!=NULL ? &allModes->decomp : NULL;
}

const Normalizer2 *Normalizer2Factory::getNFKC_CFInstance(UErrorCode &errorCode) {
    Norm2AllModes *allModes=
        Norm2AllModesSingleton(nfkc_cfSingleton, "nfkc_cf").getInstance(errorCode);
    return allModes!=NULL ? &allModes->comp : NULL;
}

const Normalizer2 *Normalizer2Factory::getNoopInstance(UErrorCode &errorCode) {
    return Norm2Singleton(noopSingleton).getInstance(errorCode);
}

const Normalizer2 *
Normalizer2Factory::getInstance(UNormalizationMode mode, UErrorCode &errorCode) {
    if(U_FAILURE(errorCode)) {
        return NULL;
    }
    switch(mode) {
    case UNORM_NFD:
        return getNFDInstance(errorCode);
    case UNORM_NFKD:
        return getNFKDInstance(errorCode);
    case UNORM_NFC:
        return getNFCInstance(errorCode);
    case UNORM_NFKC:
        return getNFKCInstance(errorCode);
    case UNORM_FCD:
        return getFCDInstance(errorCode);
    default:  // UNORM_NONE
        return getNoopInstance(errorCode);
    }
}

const Normalizer2Impl *
Normalizer2Factory::getNFCImpl(UErrorCode &errorCode) {
    Norm2AllModes *allModes=
        Norm2AllModesSingleton(nfcSingleton, "nfc").getInstance(errorCode);
    return allModes!=NULL ? &allModes->impl : NULL;
}

const Normalizer2Impl *
Normalizer2Factory::getNFKC_CFImpl(UErrorCode &errorCode) {
    Norm2AllModes *allModes=
        Norm2AllModesSingleton(nfkc_cfSingleton, "nfkc_cf").getInstance(errorCode);
    return allModes!=NULL ? &allModes->impl : NULL;
}

const Normalizer2 *
Normalizer2::getInstance(const char *packageName,
                         const char *name,
                         UNormalization2Mode mode,
                         UErrorCode &errorCode) {
    if(U_FAILURE(errorCode)) {
        return NULL;
    }
    if(packageName==NULL) {
        Norm2AllModes *allModes=NULL;
        if(0==uprv_strcmp(name, "nfc")) {
            allModes=Norm2AllModesSingleton(nfcSingleton, "nfc").getInstance(errorCode);
        } else if(0==uprv_strcmp(name, "nfkc")) {
            allModes=Norm2AllModesSingleton(nfkcSingleton, "nfkc").getInstance(errorCode);
        } else if(0==uprv_strcmp(name, "nfkc_cf")) {
            allModes=Norm2AllModesSingleton(nfkc_cfSingleton, "nfkc_cf").getInstance(errorCode);
        }
        if(allModes!=NULL) {
            switch(mode) {
            case UNORM2_COMPOSE:
                return &allModes->comp;
            case UNORM2_DECOMPOSE:
                return &allModes->decomp;
            case UNORM2_FCD:
                allModes->impl.getFCDTrie(errorCode);
                return &allModes->fcd;
            case UNORM2_COMPOSE_CONTIGUOUS:
                return &allModes->fcc;
            default:
                break;  // do nothing
            }
        }
    }
    if(U_SUCCESS(errorCode)) {
        // TODO: Real loading and caching...
        errorCode=U_UNSUPPORTED_ERROR;
    }
    return NULL;
}

UOBJECT_DEFINE_ABSTRACT_RTTI_IMPLEMENTATION(Normalizer2)

// C API ------------------------------------------------------------------- ***

U_DRAFT const UNormalizer2 * U_EXPORT2
unorm2_getInstance(const char *packageName,
                   const char *name,
                   UNormalization2Mode mode,
                   UErrorCode *pErrorCode) {
    return (const UNormalizer2 *)Normalizer2::getInstance(packageName, name, mode, *pErrorCode);
}

U_DRAFT void U_EXPORT2
unorm2_close(UNormalizer2 *norm2) {
    delete (Normalizer2 *)norm2;
}

U_DRAFT int32_t U_EXPORT2
unorm2_normalize(const UNormalizer2 *norm2,
                 const UChar *src, int32_t length,
                 UChar *dest, int32_t capacity,
                 UErrorCode *pErrorCode) {
    if(U_FAILURE(*pErrorCode)) {
        return 0;
    }
    if(src==NULL || length<-1 || capacity<0 || (dest==NULL && capacity>0) || src==dest) {
        *pErrorCode=U_ILLEGAL_ARGUMENT_ERROR;
        return 0;
    }
    UnicodeString destString(dest, 0, capacity);
    const Normalizer2 *n2=(const Normalizer2 *)norm2;
    if(n2->getDynamicClassID()==Normalizer2WithImpl::getStaticClassID()) {
        // Avoid duplicate argument checking and support NUL-terminated src.
        const Normalizer2WithImpl *n2wi=(const Normalizer2WithImpl *)n2;
        ReorderingBuffer buffer(n2wi->impl, destString);
        if(buffer.init(*pErrorCode)) {
            n2wi->normalize(src, length>=0 ? src+length : NULL, buffer, *pErrorCode);
        }
    } else {
        UnicodeString srcString(FALSE, src, length);
        n2->normalize(srcString, destString, *pErrorCode);
    }
    return destString.extract(dest, capacity, *pErrorCode);
}

static int32_t
normalizeSecondAndAppend(const UNormalizer2 *norm2,
                         UChar *first, int32_t firstLength, int32_t firstCapacity,
                         const UChar *second, int32_t secondLength,
                         UBool doNormalize,
                         UErrorCode *pErrorCode) {
    if(U_FAILURE(*pErrorCode)) {
        return 0;
    }
    if( second==NULL || secondLength<-1 ||
        firstCapacity<0 || (first==NULL && firstCapacity>0) ||
        first==second
    ) {
        *pErrorCode=U_ILLEGAL_ARGUMENT_ERROR;
        return 0;
    }
    UnicodeString firstString(first, firstLength, firstCapacity);
    const Normalizer2 *n2=(const Normalizer2 *)norm2;
    if(n2->getDynamicClassID()==Normalizer2WithImpl::getStaticClassID()) {
        // Avoid duplicate argument checking and support NUL-terminated src.
        const Normalizer2WithImpl *n2wi=(const Normalizer2WithImpl *)n2;
        ReorderingBuffer buffer(n2wi->impl, firstString);
        if(buffer.init(*pErrorCode)) {
            n2wi->normalizeAndAppend(second, secondLength>=0 ? second+secondLength : NULL,
                                     doNormalize, buffer, *pErrorCode);
        }
    } else {
        UnicodeString secondString(FALSE, second, secondLength);
        if(doNormalize) {
            n2->normalizeSecondAndAppend(firstString, secondString, *pErrorCode);
        } else {
            n2->append(firstString, secondString, *pErrorCode);
        }
    }
    return firstString.extract(first, firstCapacity, *pErrorCode);
}

U_DRAFT int32_t U_EXPORT2
unorm2_normalizeSecondAndAppend(const UNormalizer2 *norm2,
                                UChar *first, int32_t firstLength, int32_t firstCapacity,
                                const UChar *second, int32_t secondLength,
                                UErrorCode *pErrorCode) {
    return normalizeSecondAndAppend(norm2,
                                    first, firstLength, firstCapacity,
                                    second, secondLength,
                                    TRUE, pErrorCode);
}

U_DRAFT int32_t U_EXPORT2
unorm2_append(const UNormalizer2 *norm2,
              UChar *first, int32_t firstLength, int32_t firstCapacity,
              const UChar *second, int32_t secondLength,
              UErrorCode *pErrorCode) {
    return normalizeSecondAndAppend(norm2,
                                    first, firstLength, firstCapacity,
                                    second, secondLength,
                                    FALSE, pErrorCode);
}

U_DRAFT UBool U_EXPORT2
unorm2_isNormalized(const UNormalizer2 *norm2,
                    const UChar *s, int32_t length,
                    UErrorCode *pErrorCode) {
    if(U_FAILURE(*pErrorCode)) {
        return 0;
    }
    if(s==NULL || length<-1) {
        *pErrorCode=U_ILLEGAL_ARGUMENT_ERROR;
        return 0;
    }
    UnicodeString sString(FALSE, s, length);
    return ((const Normalizer2 *)norm2)->isNormalized(sString, *pErrorCode);
}

U_DRAFT UNormalizationCheckResult U_EXPORT2
unorm2_quickCheck(const UNormalizer2 *norm2,
                  const UChar *s, int32_t length,
                  UErrorCode *pErrorCode) {
    if(U_FAILURE(*pErrorCode)) {
        return UNORM_NO;
    }
    if(s==NULL || length<-1) {
        *pErrorCode=U_ILLEGAL_ARGUMENT_ERROR;
        return UNORM_NO;
    }
    UnicodeString sString(FALSE, s, length);
    return ((const Normalizer2 *)norm2)->quickCheck(sString, *pErrorCode);
}

U_DRAFT int32_t U_EXPORT2
unorm2_spanQuickCheckYes(const UNormalizer2 *norm2,
                         const UChar *s, int32_t length,
                         UErrorCode *pErrorCode) {
    if(U_FAILURE(*pErrorCode)) {
        return 0;
    }
    if(s==NULL || length<-1) {
        *pErrorCode=U_ILLEGAL_ARGUMENT_ERROR;
        return 0;
    }
    UnicodeString sString(FALSE, s, length);
    return ((const Normalizer2 *)norm2)->spanQuickCheckYes(sString, *pErrorCode);
}

U_DRAFT UBool U_EXPORT2
unorm2_hasBoundaryBefore(const UNormalizer2 *norm2, UChar32 c) {
    return ((const Normalizer2 *)norm2)->hasBoundaryBefore(c);
}

U_DRAFT UBool U_EXPORT2
unorm2_hasBoundaryAfter(const UNormalizer2 *norm2, UChar32 c) {
    return ((const Normalizer2 *)norm2)->hasBoundaryAfter(c);
}

U_DRAFT UBool U_EXPORT2
unorm2_isInert(const UNormalizer2 *norm2, UChar32 c) {
    return ((const Normalizer2 *)norm2)->isInert(c);
}

U_NAMESPACE_END

#endif  // !UCONFIG_NO_NORMALIZATION
