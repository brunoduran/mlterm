/*
 *	$Id$
 */

#ifndef  __VERSION_H__
#define  __VERSION_H__


#include  <kiklib/kik_util.h>


#define  MAJOR_VERSION	3
#define  MINOR_VERSION	5
#define  REVISION	0
#define  PATCH_LEVEL	0

#if  0
#define  CHANGE_DATE  "pre/@CHANGE_DATE@"
#elif  0
#define  CHANGE_DATE  "post/@CHANGE_DATE@"
#else
#define  CHANGE_DATE  ""
#endif


#define  VERSION \
	KIK_INT_TO_STR(MAJOR_VERSION) "." KIK_INT_TO_STR(MINOR_VERSION) "." \
	KIK_INT_TO_STR(REVISION)

#if  PATCH_LEVEL == 0
#define  DETAIL_VERSION VERSION " " CHANGE_DATE
#else
#define  DETAIL_VERSION VERSION " patch level " KIK_INT_TO_STR(PATCH_LEVEL) " " CHANGE_DATE
#endif


#endif
