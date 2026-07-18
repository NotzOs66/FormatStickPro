#ifndef _EXT2_TYPES_H
#define _EXT2_TYPES_H

#include <stdint.h>
#include <sys/types.h>

/* Use Linux types if possible */
#if defined(__linux__) || defined(__ANDROID__)
#include <linux/types.h>
#include <asm/types.h>
#endif

/*
 * Android/Bionic and some Linux systems define __u8, __u16, __u32, __u64
 * in <linux/types.h>. The guards used can be _LINUX_TYPES_H or _UAPI_LINUX_TYPES_H.
 * We also check for _UAPI_ASM_GENERIC_INT_LL64_H which is where they are often actually defined.
 */
#if !defined(_LINUX_TYPES_H) && !defined(_UAPI_LINUX_TYPES_H) && \
    !defined(_ASM_GENERIC_INT_LL64_H) && !defined(_UAPI_ASM_GENERIC_INT_LL64_H)

#ifndef HAVE___U8
#define HAVE___U8
typedef uint8_t __u8;
#endif

#ifndef HAVE___S8
#define HAVE___S8
typedef int8_t __s8;
#endif

#ifndef HAVE___U16
#define HAVE___U16
typedef uint16_t __u16;
#endif

#ifndef HAVE___S16
#define HAVE___S16
typedef int16_t __s16;
#endif

#ifndef HAVE___U32
#define HAVE___U32
typedef uint32_t __u32;
#endif

#ifndef HAVE___S32
#define HAVE___S32
typedef int32_t __s32;
#endif

#ifndef HAVE___U64
#define HAVE___U64
typedef uint64_t __u64;
#endif

#ifndef HAVE___S64
#define HAVE___S64
typedef int64_t __s64;
#endif

#endif /* guards */

/* endian checking stuff */
#ifndef EXT2_ENDIAN_H_
#define EXT2_ENDIAN_H_

/*
 * If linux/types.h was included, these might already be defined.
 * However, linux/types.h defines them as typedefs, so we can't check with #ifndef.
 * BUT, if _UAPI_LINUX_TYPES_H is defined, they are likely defined.
 */
#if !defined(_LINUX_TYPES_H) && !defined(_UAPI_LINUX_TYPES_H)

#ifndef __bitwise
#define __bitwise
#endif

#ifndef __le16
typedef uint16_t __le16;
#endif
#ifndef __le32
typedef uint32_t __le32;
#endif
#ifndef __le64
typedef uint64_t __le64;
#endif

#ifndef __be16
typedef uint16_t __be16;
#endif
#ifndef __be32
typedef uint32_t __be32;
#endif
#ifndef __be64
typedef uint64_t __be64;
#endif

#endif /* linux guards */

#endif /* EXT2_ENDIAN_H_ */

typedef __u32 blk_t;
typedef __u32 dgrp_t;
typedef __s32 ext2_off_t;
typedef __u64 blk64_t;

#endif
