/*
  +----------------------------------------------------------------------+
  | PHP Version 5                                                        |
  +----------------------------------------------------------------------+
  | Copyright (c) 1997-2007 The PHP Group                                |
  +----------------------------------------------------------------------+
  | This source file is subject to version 3.01 of the PHP license,      |
  | that is bundled with this package in the file LICENSE, and is        |
  | available through the world-wide-web at the following url:           |
  | http://www.php.net/license/3_01.txt                                  |
  | If you did not receive a copy of the PHP license and are unable to   |
  | obtain it through the world-wide-web, please send a note to          |
  | license@php.net so we can mail you a copy immediately.               |
  +----------------------------------------------------------------------+
  | Author: SZG                                                             |
  +----------------------------------------------------------------------+
*/

/* $Id: header,v 1.16.2.1.2.1 2007/01/01 19:32:09 iliaa Exp $ */

extern "C" {
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "php.h"
#include "php_ini.h"
#include "ext/standard/info.h"
#include "php_xsplit.h"
#include <fcntl.h>
#ifdef PHP_WIN32
#  include <windows.h>
#  include <io.h>
#else


#include <sys/mman.h>
#endif
}

#include <fstream>
#include <iostream>
#include <string>
#include <vector>
#include <math.h>
#include "darts-clone.ex.h"
#include "lookup3.h"

using namespace std;
unsigned char UTF8CharLenTable[] =
{
        /*
        1-4:    a leading byte of UTF-8 sequence, and the value is of its length
        8:              a content byte(Not a leading byte)
        16:             illegal byte, it never apperas in a UTF-8 sequence
        */
        1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, //0-15
        1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, //16-31
        1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, //32-47
        1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, //48-63

        1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, //64-79
        1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, //80-95
        1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, //96-111
        1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, //112-127

        8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
        8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
        8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
        8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,

        16, 16, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, //192-207
        2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, //208-223

        3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, //224-239

        4, 4, 4, 4, 4, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16  //240-255
};

Darts::DoubleArray da;

#define SIZE_OF_RESULT_PAIR 256


/* If you declare any globals in php_xsplit.h uncomment this: */
ZEND_DECLARE_MODULE_GLOBALS(xsplit)

#define SAFE_STRING(s) ((s)?(s):"")
#define CHECK_DICT(dict_id) { if (dict_id==-1) { php_error(E_ERROR,"The dict can not be opened"); RETURN_FALSE; } }
struct _dict {
  void *mmapped_addr;
	size_t size;
};

/* True global resources - no need for thread safety here */
static int le_xsplit_dict, le_xsplit_pdict;

/* {{{ xsplit_functions[]
 *
 * Every user visible function must have an entry in xsplit_functions[].
 */
zend_function_entry xsplit_functions[] = {
	PHP_FE(xs_open,	NULL)
	PHP_FE(xs_build,	NULL)
	PHP_FE(xs_split,   	NULL)
	PHP_FE(xs_search,	    NULL)
	PHP_FE(xs_simhash,	    NULL)
	PHP_FE(xs_simhash_wide,	    NULL)
	PHP_FE(xs_hdist,	    NULL)
	{NULL, NULL, NULL}	/* Must be the last line in xsplit_functions[] */
};
/* }}} */

/* {{{ xsplit_module_entry
 */
zend_module_entry xsplit_module_entry = {
#if ZEND_MODULE_API_NO >= 20010901
	STANDARD_MODULE_HEADER,
#endif
	"xsplit",
	xsplit_functions,
	PHP_MINIT(xsplit),
	PHP_MSHUTDOWN(xsplit),
	PHP_RINIT(xsplit),		/* Replace with NULL if there's nothing to do at request start */
	PHP_RSHUTDOWN(xsplit),	/* Replace with NULL if there's nothing to do at request end */
	PHP_MINFO(xsplit),
#if ZEND_MODULE_API_NO >= 20010901
	"0.1", /* Replace with version number for your extension */
#endif
	STANDARD_MODULE_PROPERTIES
};
/* }}} */

#ifdef COMPILE_DL_XSPLIT
ZEND_GET_MODULE(xsplit)
#endif

/* {{{ set default dictionary so that it can be called if dicitonary is not specified
 */
static void _set_default_dict(int id TSRMLS_DC)
{
	if (XSPLIT_G(default_dict) != -1) {
		zend_list_delete(XSPLIT_G(default_dict));
	}
	XSPLIT_G(default_dict) = id;
	zend_list_addref(id);

}
/* }}} */

/* {{{ close non-persistent dictionary, also used as a destructor
 */
static void _close_xsplit_dict(zend_rsrc_list_entry *rsrc TSRMLS_DC)
{
	_dict *dict = (_dict *)rsrc->ptr;
    /* ummap the mmapped memory */
#ifdef PHP_WIN32
	UnmapViewOfFile (dict->mmapped_addr);
#else
    munmap(dict->mmapped_addr,dict->size);
#endif
    XSPLIT_G(num_dicts)--;
}
/* }}} */

/* {{{ close persistent dictionary, also used as a destructor
 */
static void _close_xsplit_pdict(zend_rsrc_list_entry *rsrc TSRMLS_DC)
{
	_dict *dict = (_dict *)rsrc->ptr;
    /* ummap the mmapped memory */
#ifdef PHP_WIN32
	UnmapViewOfFile (dict->mmapped_addr);
#else
    munmap(dict->mmapped_addr,dict->size);
#endif
	XSPLIT_G(num_persistent)--;
	XSPLIT_G(num_dicts)--;
}
/* }}} */

/* {{{ open dictionary
 */
static void _open_xsplit_dict(INTERNAL_FUNCTION_PARAMETERS, char *dict_file, size_t dict_file_len, zend_bool persistent) {
    int fd;
    size_t file_size;
    void *mmapped_dict;
    _dict *dict_ptr;
	char *hashed_details=NULL;
	int hashed_details_length;
    hashed_details_length = spprintf(&hashed_details, 0, "xsplit_%s", SAFE_STRING(dict_file));
	if (!XSPLIT_G(allow_persistent)) {
		persistent=0;
	}
	if (persistent) {
		zend_rsrc_list_entry *le;
		/* try to find if we already have this dict in our persistent list */
		if (zend_hash_find(&EG(persistent_list), hashed_details, hashed_details_length+1, (void **) &le)==FAILURE) {  /* we don't */
			zend_rsrc_list_entry new_le;

			if (XSPLIT_G(max_dicts)!=-1 && XSPLIT_G(num_dicts)>=XSPLIT_G(max_dicts)) {
				php_error_docref(NULL TSRMLS_CC, E_WARNING, "Too many opened dicts (%ld)", XSPLIT_G(num_dicts));
				efree(hashed_details);
                RETURN_FALSE;
			}
			if (XSPLIT_G(max_persistent)!=-1 && XSPLIT_G(num_persistent)>=XSPLIT_G(max_persistent)) {
				php_error_docref(NULL TSRMLS_CC, E_WARNING, "Too many opened persistent dicts (%ld)", XSPLIT_G(num_persistent));
				efree(hashed_details);
                RETURN_FALSE;
			}
            /*creating new mmapped dict*/
#ifdef PHP_WIN32
    		struct _stat sb;
		    if (_stat (dict_file, &sb) == -1)
#else
		    struct stat sb;
		    if (stat (dict_file, &sb) == -1)
#endif
				return;

#ifdef PHP_WIN32
		    HANDLE mmf = 
    		    CreateFile (dict_file, GENERIC_READ, FILE_SHARE_READ, NULL,
    		                OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);

    		if (mmf == INVALID_HANDLE_VALUE)
    		    return;
                              
    		HANDLE mmfv = 
    		    CreateFileMapping (mmf, NULL, PAGE_READONLY, 0, 0, NULL);
    		if (mmfv == INVALID_HANDLE_VALUE) {
    		    CloseHandle (mmf);
    		    return;
    		}

    		mmapped_dict = 
    		    MapViewOfFile (mmfv, FILE_MAP_READ, 0, 0, sb.st_size);

    // The handles can be safely closed
    		CloseHandle (mmf);
    		CloseHandle (mmfv);
#else
    	    if((fd = open(dict_file, O_RDONLY)) == -1) {
    	        php_error(E_ERROR, "Can not open dictionary file");
    	        RETURN_FALSE;
    	    }
    	    file_size = lseek(fd, 0, SEEK_END);

   		    if(!(mmapped_dict = mmap(NULL, sb.st_size, PROT_READ, MAP_SHARED, fd, 0))) {
   		        php_error(E_ERROR, "Can not map the dictionary into memory");
   		        RETURN_FALSE;
   		    }
#endif
    		dict_ptr = (_dict *) malloc(sizeof(_dict));
    		dict_ptr->mmapped_addr=mmapped_dict;
    		dict_ptr->size=sb.st_size;


            /* hash it up */
            Z_TYPE(new_le) = le_xsplit_pdict;

			new_le.ptr = dict_ptr;
			if (zend_hash_update(&EG(persistent_list), hashed_details, hashed_details_length+1, (void *) &new_le, sizeof(zend_rsrc_list_entry), NULL)==FAILURE) {
                RETURN_FALSE;	
			}
			XSPLIT_G(num_persistent)++;
			XSPLIT_G(num_dicts)++;
		} else {  /* The dict is in our list of persistent connections */
			if (Z_TYPE_P(le) != le_xsplit_pdict) {
				RETURN_FALSE;
			}
            dict_ptr=(_dict *)le->ptr;
            
        }
        ZEND_REGISTER_RESOURCE(return_value, dict_ptr, le_xsplit_pdict);
    } else {/* non persistent */
		zend_rsrc_list_entry *index_ptr, new_index_ptr;
		
		if (zend_hash_find(&EG(regular_list), hashed_details, hashed_details_length+1,(void **) &index_ptr)==SUCCESS) {
			int type;
			long dict;
			void *ptr;
			if (Z_TYPE_P(index_ptr) != le_index_ptr) {
				RETURN_FALSE;
			}
			dict = (long) index_ptr->ptr;
			ptr = zend_list_find(dict,&type);   /* check if the link is still there */
			if (ptr && (type==le_xsplit_dict || type==le_xsplit_pdict)) {
				zend_list_addref(dict);
				Z_LVAL_P(return_value) = dict;
                _set_default_dict(dict TSRMLS_CC);
				Z_TYPE_P(return_value) = IS_RESOURCE;
				efree(hashed_details);
				return;
			} else {
				zend_hash_del(&EG(regular_list), hashed_details, hashed_details_length+1);
			}
		}
        /* check if over the max opend dictionaries */
		if (XSPLIT_G(max_dicts)!=-1 && XSPLIT_G(num_dicts)>=XSPLIT_G(max_dicts)) {
			php_error_docref(NULL TSRMLS_CC, E_WARNING, "Too many opened dicts (%ld)", XSPLIT_G(num_dicts));
			efree(hashed_details);
			//MYSQL_DO_CONNECT_RETURN_FALSE();
            RETURN_FALSE;
		}
        /*creating new mmapped dict*/
#ifdef PHP_WIN32
    struct _stat sb;
    if (_stat (dict_file, &sb) == -1)
#else
    struct stat sb;
    if (stat (dict_file, &sb) == -1)
#endif
		return;

#ifdef PHP_WIN32
    HANDLE mmf = 
        CreateFile (dict_file, GENERIC_READ, FILE_SHARE_READ, NULL,
                    OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);

    if (mmf == INVALID_HANDLE_VALUE)
        return;
                              
    HANDLE mmfv = 
        CreateFileMapping (mmf, NULL, PAGE_READONLY, 0, 0, NULL);
    if (mmfv == INVALID_HANDLE_VALUE) {
        CloseHandle (mmf);
        return;
    }

    mmapped_dict = 
        MapViewOfFile (mmfv, FILE_MAP_READ, 0, 0, sb.st_size);

    // The handles can be safely closed
    CloseHandle (mmf);
    CloseHandle (mmfv);
#else
        if((fd = open(dict_file, O_RDONLY)) == -1) {
            php_error(E_ERROR, "Can not open dictionary file");
            RETURN_FALSE;
        }
        file_size = lseek(fd, 0, SEEK_END);

        if(!(mmapped_dict = mmap(NULL, sb.st_size, PROT_READ, MAP_SHARED, fd, 0))) {
             php_error(E_ERROR, "Can not map the dictionary into memory");
            RETURN_FALSE;
        }
#endif
        dict_ptr = (_dict *) malloc(sizeof(_dict));
        dict_ptr->mmapped_addr=mmapped_dict;
        dict_ptr->size=sb.st_size;

		/* add it to the list */
		ZEND_REGISTER_RESOURCE(return_value, dict_ptr, le_xsplit_dict);

		/* add it to the hash */
		new_index_ptr.ptr = (void *) Z_LVAL_P(return_value);
		Z_TYPE(new_index_ptr) = le_index_ptr;
		if (zend_hash_update(&EG(regular_list), hashed_details, hashed_details_length+1,(void *) &new_index_ptr, sizeof(zend_rsrc_list_entry), NULL)==FAILURE) {
			efree(hashed_details);
            RETURN_FALSE;
		}
		XSPLIT_G(num_dicts)++;


    }
	efree(hashed_details);
	_set_default_dict(Z_LVAL_P(return_value) TSRMLS_CC);
}
/* }}} */

/* {{{ get default dictionary
 */
static long _get_default_dict(INTERNAL_FUNCTION_PARAMETERS)
{
	char ini_file[] = "xsplithdefault_dict_file";
	if (XSPLIT_G(default_dict) == -1) {  /*no dictionary opened yet, implicitly open one */
        /* get default dictionary path specified in php.ini */
        char *dict_file=INI_STR(ini_file);
        _open_xsplit_dict(INTERNAL_FUNCTION_PARAM_PASSTHRU, dict_file, strlen(dict_file), 1);
	}
	return XSPLIT_G(default_dict);

}
/* }}} */

/* {{{ compare function, used in hash sorting
 */
int getBestWordLength(Darts::DoubleArray::key_type *pText, size_t iTextLength) {
    Darts::DoubleArray::result_pair_type  dResultPair_A[64];
    Darts::DoubleArray::result_pair_type  dResultPair_B[64];
    Darts::DoubleArray::result_pair_type  dResultPair_C[64];

    size_t iNum_A, iNum_B, iNum_C; //A B C stand for the first, sencod, third word in a chunk
    register size_t iCur_A, iCur_B, iCur_C;
    Darts::DoubleArray::key_type *pText_A, *pText_B, *pText_C;
    size_t iTextLength_A, iTextLength_B, iTextLength_C;
    
    //we use a float 10-element-array to store a chunk
    //0-word count
    //1-3 byte length of each word
    //4-6 frequence of each word
    //7-9 character count of each word

    float dChunks[64][10]={{0}};
    size_t iChunkCur=0;
    size_t iChunkCount;

    float dChunk_A[10]={0};
    float dChunk_B[10]={0};

    size_t iByteCount;
    register size_t iByteCur;
    size_t iChunkSize=sizeof(float)*10;        

    size_t dClip[64];//temporary cursor data when filtering
    size_t dBest[64];//the best result. There may be more than 1 chunk at last, so we use array
    register size_t iClipCur=0;
    size_t iClipSize=0;
    register size_t iBestCur=0;
    size_t iResultSize = sizeof(int)*64;

    float dLengths[64]={0}; //chunks' lengths
    float dWordCounts[64]={0}; //for chunks' average lengths, but we use word count when filtering with this rule
    float dVariances[64]={0}; //chunks' variances
    float dSDMFs[64]={0}; //chunks' sum of degree of morphemic freedom of one-character words

    float iMaxChunkLength=0;
    float fMinWordCount;//used to filtering with max average length, which also means having the minimal word count;
    float fMinVariance;
	float fAvgLength;
    int iTemp; //mostly for filtering with min variance
    float fTemp; ////mostly for filtering with min variance
    float fMaxSDMF=0;

    pText_A=pText;
    iTextLength_A=iTextLength;
    //get all the chunks
    iNum_A = da.commonPrefixSearch(pText_A, dResultPair_A, 64, iTextLength_A);
    for(iCur_A=0;iCur_A<iNum_A;iCur_A++) {
        dChunk_A[0] = 1;
        dChunk_A[1] = dResultPair_A[iCur_A].length;
        dChunk_A[4] = dResultPair_A[iCur_A].value;
        //character count
        iByteCount=0;
        for(iByteCur=0;iByteCur<dChunk_A[1];iByteCur++) {
			if(UTF8CharLenTable[(unsigned char)(*(pText_A+iByteCur))]<0x08) {
                iByteCount++;
            }
        }
        dChunk_A[7] = iByteCount;

        pText_B = pText_A + dResultPair_A[iCur_A].length;
        iTextLength_B = iTextLength_A - dResultPair_A[iCur_A].length;

        iNum_B = da.commonPrefixSearch(pText_B, dResultPair_B, 64, iTextLength_B);
        if(iNum_B) {        
            for(iCur_B=0;iCur_B<iNum_B;iCur_B++) {
                memcpy(dChunk_B, dChunk_A, iChunkSize);//initialize chunk B
                dChunk_B[0] = 2;            
                dChunk_B[2] = dResultPair_B[iCur_B].length;
                dChunk_B[5] = dResultPair_B[iCur_B].value;
                ///character count
                iByteCount=0;
                for(iByteCur=0;iByteCur<dChunk_B[2];iByteCur++) {
				if(UTF8CharLenTable[(unsigned char)(*(pText_B+iByteCur))]<0x08) {
                        iByteCount++;
                    }
                }
                dChunk_B[8] = iByteCount;

                pText_C = pText_B + dResultPair_B[iCur_B].length;
                iTextLength_C = iTextLength_B - dResultPair_B[iCur_B].length;
                iNum_C = da.commonPrefixSearch(pText_C , dResultPair_C, 64, iTextLength_C);
                if(iNum_C) {
                    for(iCur_C=0;iCur_C<iNum_C;iCur_C++) {
                        memcpy(dChunks[iChunkCur], dChunk_B, iChunkSize);

                        dChunks[iChunkCur][0] = 3;
                        dChunks[iChunkCur][3] = dResultPair_C[iCur_C].length;
                        dChunks[iChunkCur][6] = dResultPair_C[iCur_C].value;
                        //character count
                        iByteCount=0;
                        for(iByteCur=0;iByteCur<dChunks[iChunkCur][3];iByteCur++) {
						if(UTF8CharLenTable[(unsigned char)(*(pText_C+iByteCur))]<0x08) {
                                iByteCount++;
                            }
                        }
                        dChunks[iChunkCur][9] = iByteCount;

                        iChunkCur++;
                    }
                } else {
                    memcpy(dChunks[iChunkCur], dChunk_B, iChunkSize);
                    iChunkCur++;
                }//if(iNum_C)
            }
        } else {
            memcpy(dChunks[iChunkCur], dChunk_A, iChunkSize);
            iChunkCur++;
        }//if(iNum_B)
    }//the main loop of obtaining chunks

    iChunkCount=iChunkCur;//just remember the chunks count

    //filter with max length;
    for(iChunkCur=0;iChunkCur<iChunkCount;iChunkCur++) {
        dLengths[iChunkCur] = dChunks[iChunkCur][7] + dChunks[iChunkCur][8] + dChunks[iChunkCur][9];
        iMaxChunkLength = iMaxChunkLength > dLengths[iChunkCur] ? iMaxChunkLength : dLengths[iChunkCur];
    }
    for(iChunkCur=0;iChunkCur<iChunkCount;iChunkCur++) {
        if(iMaxChunkLength == dLengths[iChunkCur]) {
            dBest[iBestCur++]=iChunkCur;
        }
    }
    if(iBestCur<2) {
        return (int)dChunks[dBest[0]][1];
    }

    //filter with average word legnth

    memcpy(dClip, dBest, iResultSize);
    iClipSize=iBestCur;

	//initialize the first one
	dWordCounts[dClip[0]] = dChunks[dClip[0]][0];
	fMinWordCount=dChunks[dClip[0]][0];

    for(iClipCur=1;iClipCur<iClipSize;iClipCur++) {//notice that iClipCur starts from 1 (not 0)
        dWordCounts[dClip[iClipCur]] = dChunks[dClip[iClipCur]][0];
        fMinWordCount = fMinWordCount < dWordCounts[dClip[iClipCur]] ? fMinWordCount : dWordCounts[dClip[iClipCur]];
    }
    iBestCur=0;
    for(iClipCur=0;iClipCur<iClipSize;iClipCur++) {
        if(fMinWordCount == dWordCounts[dClip[iClipCur]]) {
            dBest[iBestCur++]=dClip[iClipCur];
        }
    }
    if(iBestCur<2) {
        return (int)dChunks[dBest[0]][1];
    }

    //filter with min variance
    
    memcpy(dClip, dBest, iResultSize);
    iClipSize=iBestCur;

	//initialize the first one
	fAvgLength = (dChunks[dClip[0]][7]+dChunks[dClip[0]][8]+dChunks[dClip[0]][9])/dChunks[dClip[0]][0];
    for(iTemp=0;iTemp<dChunks[dClip[0]][0];iTemp++) {
        fTemp= fAvgLength - dChunks[dClip[0]][iTemp+7];
        dVariances[dClip[0]] += fTemp*fTemp;
    }
    dVariances[dClip[0]] = sqrt(dVariances[dClip[0]]/dChunks[dClip[0]][0]);
    fMinVariance = dVariances[dClip[0]];

    for(iClipCur=1;iClipCur<iClipSize;iClipCur++) {//notice that iClipCur starts from 1, not 0
		fAvgLength = (dChunks[dClip[iClipCur]][7]+dChunks[dClip[iClipCur]][8]+dChunks[dClip[iClipCur]][9])/dChunks[dClip[iClipCur]][0];
        for(iTemp=0;iTemp<dChunks[dClip[iClipCur]][0];iTemp++) {
            fTemp = fAvgLength - dChunks[dClip[iClipCur]][iTemp+7];
            dVariances[dClip[iClipCur]] += fTemp*fTemp;
        }
        dVariances[dClip[iClipCur]] = sqrt(dVariances[dClip[iClipCur]]/dChunks[dClip[iClipCur]][0]);
        fMinVariance = fMinVariance < dVariances[dClip[iClipCur]] ? fMinVariance : dVariances[dClip[iClipCur]];
    }
    iBestCur=0;
    for(iClipCur=0;iClipCur<iClipSize;iClipCur++) {
        if(fMinVariance == dVariances[dClip[iClipCur]]) {
            dBest[iBestCur++]=dClip[iClipCur];
        }
    }
    if(iBestCur<2) {
        return (int)dChunks[dBest[0]][1];
    }

    //filter with max SDMF(sum of degree of morphemic freedom of one-character words)

    memcpy(dClip, dBest, iResultSize);
    iClipSize=iBestCur;

    for(iClipCur=0;iClipCur<iClipSize;iClipCur++) {
        for(iTemp=0;iTemp<dChunks[dClip[iClipCur]][0];iTemp++) {
            if(dChunks[dClip[iClipCur]][iTemp+7]==1) {
                dSDMFs[dClip[iClipCur]]+=log(dChunks[dClip[iClipCur]][iTemp+4]);
            }
        }
        fMaxSDMF = fMaxSDMF > dSDMFs[dClip[iClipCur]] ? fMaxSDMF : dSDMFs[dClip[iClipCur]];
    }
    iBestCur=0;
    for(iClipCur=0;iClipCur<iClipSize;iClipCur++) {
        if(fMaxSDMF == dSDMFs[dClip[iClipCur]]) {
            dBest[iBestCur++]=dClip[iClipCur];
        }
    }
    //if(iBestCur<2) {
    return (int)dChunks[dBest[0]][1];
    //}    
}
/* }}} */


/* {{{ compare function, used in hash sorting
 */
static int array_key_compare(const void *a, const void *b TSRMLS_DC) /* {{{ */
{
	Bucket *f;
	Bucket *s;
	zval result;
	zval first;
	zval second;
 
	f = *((Bucket **) a);
	s = *((Bucket **) b);

	if (f->nKeyLength == 0) {
		Z_TYPE(first) = IS_LONG;
		Z_LVAL(first) = f->h;
	} else {
		Z_TYPE(first) = IS_STRING;
		Z_STRVAL(first) = const_cast<char*>(f->arKey);
		Z_STRLEN(first) = f->nKeyLength-1;
	}

	if (s->nKeyLength == 0) {
		Z_TYPE(second) = IS_LONG;
		Z_LVAL(second) = s->h;
	} else {
		Z_TYPE(second) = IS_STRING;
		Z_STRVAL(second) = const_cast<char*>(s->arKey);
		Z_STRLEN(second) = s->nKeyLength-1;
	}
	if (compare_function(&result, &first, &second TSRMLS_CC) == FAILURE) {
	//if (XSPLIT_G(compare_func)(&result, &first, &second TSRMLS_CC) == FAILURE) {
		return 0;
	} 

	if (Z_TYPE(result) == IS_DOUBLE) {
		if (Z_DVAL(result) < 0) {
			return -1;
		} else if (Z_DVAL(result) > 0) {
			return 1;
		} else {
			return 0;
		}
	}

	convert_to_long(&result);

	if (Z_LVAL(result) < 0) {
		return -1;
	} else if (Z_LVAL(result) > 0) {
		return 1;
	} 

	return 0;
}
/* }}} */


/* {{{ hash function for simhash
 */
/** Hashes the given token.
 */
static uint64_t
get_hashed_token( const char *token_start, size_t token_length)
{
        uint32_t h1 = 0xac867c1d; /* Dummy init values */
        uint32_t h2 = 0x5434e4c4;
        hashlittle2( token_start, token_length, &h1, &h2);
        return ((uint64_t)h1 << 32) +h2;
}
/* }}} */



/** Updates histogram with weight==1.
 * Tricky implementation, optimized for speed.
 */
static inline void
update_hist( int hist[], uint64_t token)
{
#define HIST    *hist++ += t & 1; t >>= 1;
        uint32_t t = token;
        HIST HIST HIST HIST HIST HIST HIST HIST
        HIST HIST HIST HIST HIST HIST HIST HIST
        HIST HIST HIST HIST HIST HIST HIST HIST
        HIST HIST HIST HIST HIST HIST HIST
        *hist++ += t & 1;
        t = token>>32;
        HIST HIST HIST HIST HIST HIST HIST HIST
        HIST HIST HIST HIST HIST HIST HIST HIST
        HIST HIST HIST HIST HIST HIST HIST HIST
        HIST HIST HIST HIST HIST HIST HIST
        *hist += t & 1;
}



/* {{{ PHP_INI
 */
PHP_INI_BEGIN()
	STD_PHP_INI_BOOLEAN(const_cast<char*>("xsplit.allow_persistent"),	const_cast<char*>("1"), PHP_INI_SYSTEM, OnUpdateLong, allow_persistent, zend_xsplit_globals, xsplit_globals)
	STD_PHP_INI_ENTRY(const_cast<char*>("xsplit.max_persistent"), const_cast<char*>("-1"), PHP_INI_SYSTEM, OnUpdateLong, max_persistent, zend_xsplit_globals, xsplit_globals)
	STD_PHP_INI_ENTRY(const_cast<char*>("xsplit.max_dicts"), const_cast<char*>("-1"), PHP_INI_SYSTEM, OnUpdateLong, max_dicts, zend_xsplit_globals, xsplit_globals)
	PHP_INI_ENTRY(const_cast<char*>("xsplit.default_dict_file"), const_cast<char*>("xdcit"), PHP_INI_ALL, NULL)
PHP_INI_END()
/* }}} */

/* {{{ php_xsplit_init_globals
 */
/* Uncomment this function if you have INI entries
static void php_xsplit_init_globals(zend_xsplit_globals *xsplit_globals)
{
	xsplit_globals->global_value = 0;
	xsplit_globals->global_string = NULL;
}
*/
/* }}} */

/* {{{ PHP_MINIT_FUNCTION
 */
PHP_MINIT_FUNCTION(xsplit)
{
	REGISTER_INI_ENTRIES();

    /* register resources */
	le_xsplit_dict  =   zend_register_list_destructors_ex(_close_xsplit_dict, NULL, "xsplit dict", module_number);
	le_xsplit_pdict =   zend_register_list_destructors_ex(NULL, _close_xsplit_pdict, "xsplit dict persistent", module_number);

    REGISTER_LONG_CONSTANT("XS_SPLIT_MMSEG",            1,     CONST_CS | CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("XS_SPLIT_MMFWD",            2,     CONST_CS | CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("XS_SEARCH_CP",              1,     CONST_CS | CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("XS_SEARCH_EM",              2,     CONST_CS | CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("XS_SEARCH_ALL_SIMPLE",      3,     CONST_CS | CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("XS_SEARCH_ALL_DETAIL",      4,     CONST_CS | CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("XS_SEARCH_ALL_INDICT",      5,     CONST_CS | CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("XS_SEARCH_PREDICTIVE",      6,     CONST_CS | CONST_PERSISTENT);

	return SUCCESS;
}
/* }}} */

/* {{{ PHP_MSHUTDOWN_FUNCTION
 */
PHP_MSHUTDOWN_FUNCTION(xsplit)
{
	UNREGISTER_INI_ENTRIES();
	return SUCCESS;
}
/* }}} */

/* Remove if there's nothing to do at request start */
/* {{{ PHP_RINIT_FUNCTION
 */
PHP_RINIT_FUNCTION(xsplit)
{
    /* initialize default_dict */
	XSPLIT_G(default_dict)=-1;
    /* initialize the number of opend persistent dictionary */
	XSPLIT_G(num_dicts) = XSPLIT_G(num_persistent);
	return SUCCESS;
}
/* }}} */

/* Remove if there's nothing to do at request end */
/* {{{ PHP_RSHUTDOWN_FUNCTION
 */
PHP_RSHUTDOWN_FUNCTION(xsplit)
{
	return SUCCESS;
}
/* }}} */

/* {{{ PHP_MINFO_FUNCTION
 */
PHP_MINFO_FUNCTION(xsplit)
{
	php_info_print_table_start();
	php_info_print_table_row(2, "XSplit Support", "enabled");
	php_info_print_table_row(2, "Version", "0.0.8 Beta");
	php_info_print_table_row(2, "Build", "20101102SZG");
	php_info_print_table_end();
    
	php_info_print_table_start();
	php_info_print_table_row(2, "Warning", "This release is still in BETA phase and you are using it at your own risk!");
	php_info_print_table_end();

	DISPLAY_INI_ENTRIES();
}
/* }}} */


/* {{{ 
 */
PHP_FUNCTION(xs_open)
{
    char *dict_file;
    size_t dict_file_len;
    zend_bool persistent=1;
    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s|b",
                        &dict_file, &dict_file_len, &persistent) == FAILURE) {
        RETURN_FALSE;
    }
    _open_xsplit_dict(INTERNAL_FUNCTION_PARAM_PASSTHRU, dict_file, dict_file_len, persistent);
}
/* }}} */

/* {{{ 
 */
PHP_FUNCTION(xs_build)
{
    zval *array;
    char *dict;
    int dict_len;
    HashTable *arrht;

    char *key;
    uint keylen;
    ulong idx;
    int type;
    zval **ppzval;

    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "a/s", &array, &dict, &dict_len) == FAILURE) {
        RETURN_FALSE;
    }

    arrht = HASH_OF(array);

    if (zend_hash_sort(arrht, zend_qsort, array_key_compare, 0 TSRMLS_CC) == FAILURE) {
        RETURN_FALSE;
    }

	std::vector<const char *> dkey;
	std::vector<Darts::DoubleArray::value_type > dvalue;
	//Darts::DoubleArray::value_type tdvalue;

    for(zend_hash_internal_pointer_reset(arrht); zend_hash_has_more_elements(arrht) == SUCCESS; zend_hash_move_forward(arrht)) {

        type = zend_hash_get_current_key_ex(arrht, &key, &keylen, &idx, 0, NULL);
        if (zend_hash_get_current_data(arrht, (void**)&ppzval) == FAILURE) {
            /* Should never actually fail
             * since the key is known to exist. */
            continue;
        }
        convert_to_string_ex(ppzval);
        /* Output */
        if (type == HASH_KEY_IS_STRING) {
            /* String Key / Associative */
			char *tmp = new char[keylen+1];
			strcpy(tmp, key);
			dkey.push_back(tmp);
			//dkey.push_back(key);
			
			dvalue.push_back(atoi(Z_STRVAL_PP(ppzval)));
        } else {
            /* Numeric Key */
        }
    }
	if (da.build(dkey.size(), &dkey[0], 0, &dvalue[0]) != 0
      || da.save(dict) != 0) {
		da.clear();
		RETURN_FALSE;
	}
	for (unsigned int i = 0; i < dkey.size(); i++)
    	delete [] dkey[i];

	da.clear();
    RETURN_TRUE;
}
/* }}} */

/* {{{ 
 */
PHP_FUNCTION(xs_split)
{
    Darts::DoubleArray::key_type *text;
    Darts::DoubleArray::result_pair_type  result_pair[SIZE_OF_RESULT_PAIR];
    int     split_method=1;
    int i,ii;
    int  text_len;
    int num;
    zval *dict_rsrc;
    _dict *dict_ptr;
    long dict_id=-1;


    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s|lr",
                        &text, &text_len, &split_method, &dict_rsrc) == FAILURE) {
        RETURN_FALSE;
    }
    if(ZEND_NUM_ARGS()==3) {
	ZEND_FETCH_RESOURCE2(dict_ptr, _dict *, &dict_rsrc, -1, "XSplit-Dict", le_xsplit_dict, le_xsplit_pdict);
    } else {
        dict_id = _get_default_dict(INTERNAL_FUNCTION_PARAM_PASSTHRU);
        CHECK_DICT(dict_id);
	ZEND_FETCH_RESOURCE2(dict_ptr, _dict *, NULL, dict_id, "XSplit-Dict", le_xsplit_dict, le_xsplit_pdict);
    }
    da.set_array((void *)dict_ptr->mmapped_addr);
    
    array_init(return_value);
    i=0;

    switch(split_method) {
        case 2:
            while(i<text_len){
                num = da.commonPrefixSearch((text+i), result_pair, SIZE_OF_RESULT_PAIR, text_len-i);

                if( num == 0) {
                    add_next_index_stringl(return_value, (text+i) , UTF8CharLenTable[(unsigned char)(*(text+i))], 1);
                    i+=UTF8CharLenTable[(unsigned char)(*(text+i))];
                } else {
                    add_next_index_stringl(return_value, (text+i) , result_pair[num-1].length, 1);
                    i+=result_pair[num-1].length;
                }
            }
        break;
        default:
            while(i<text_len){
                num = da.commonPrefixSearch((text+i), result_pair, SIZE_OF_RESULT_PAIR, text_len-i);

                if( num == 0) {
                    ii=UTF8CharLenTable[(unsigned char)(*(text+i))];

                    if(ii==1&& ( (*(text+i)>='a'&&*(text+i)<='z') ||(*(text+i)>='A'&&*(text+i)<='Z') || (*(text+i)>='0'&&*(text+i)<='9') || *(text+i)=='_' ) ) {
                        while(0==da.commonPrefixSearch((text+i+ii), result_pair, SIZE_OF_RESULT_PAIR, text_len-i-ii) && (1==UTF8CharLenTable[(unsigned char)(*(text+i+ii))]) && ( (*(text+i+ii)>='a'&&*(text+i+ii)<='z') ||(*(text+i+ii)>='A'&&*(text+i+ii)<='Z') || (*(text+i+ii)>='0'&&*(text+i+ii)<='9') || *(text+i+ii)=='_' )  && (i+ii)<text_len ) {
                            ++ii;
                        }
                    }
                    add_next_index_stringl(return_value, (text+i) , ii, 1);
                    i+=ii;
                } else if(num==1) {
                    add_next_index_stringl(return_value, (text+i) , result_pair[num-1].length, 1);
                    i+=result_pair[num-1].length;
                } else if(num > 1) {

		int temp=getBestWordLength((text+i), (text_len-i));
                    add_next_index_stringl(return_value, (text+i) , temp, 1);
                    i+=temp;
   
                }

            }
        break;
    }
}
/* }}} */

/* {{{ 
 */
PHP_FUNCTION(xs_search)
{
    Darts::DoubleArray::key_type *text;
    Darts::DoubleArray::result_pair_type  result_pair[SIZE_OF_RESULT_PAIR];
    Darts::DoubleArray::result_pair_type  single_result_pair;
    int search_method=1;
    int i,ii;
    int  text_len;
    int num=0,founds=0;
    zval *result_item;
    zval *dict_rsrc;
    _dict *dict_ptr;
    long dict_id=-1;
    HashTable *arrht;
    ulong hashval;
    zval **origval;


    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s|lr",
                        &text, &text_len, &search_method, &dict_rsrc) == FAILURE) {
        RETURN_FALSE;
    }

    if(ZEND_NUM_ARGS()==3) {
        //we use passed dict_rarc
	ZEND_FETCH_RESOURCE2(dict_ptr, _dict *, &dict_rsrc, -1, "XSplit-Dict", le_xsplit_dict, le_xsplit_pdict);
    } else {
        //we use default dict id
        dict_id = _get_default_dict(INTERNAL_FUNCTION_PARAM_PASSTHRU);
        CHECK_DICT(dict_id);
	ZEND_FETCH_RESOURCE2(dict_ptr, _dict *, NULL, dict_id, "XSplit-Dict", le_xsplit_dict, le_xsplit_pdict);
    }
    da.set_array((void *)dict_ptr->mmapped_addr);
    
    i=0;

    switch(search_method) {
        case 2:
            da.exactMatchSearch(text, single_result_pair);

        	if(single_result_pair.length==0 && single_result_pair.value==-1) {
                /* not found*/
		        RETVAL_FALSE;
        	} else {
	            array_init(return_value);
        		add_next_index_long(return_value, single_result_pair.value);
		        add_next_index_long(return_value, single_result_pair.length);

	        }
        break;
        case 3:
            while(i<text_len){
                num = da.commonPrefixSearch((text+i), result_pair, SIZE_OF_RESULT_PAIR, text_len-i);
    
                if( num == 0) {
                    /* not found with this prefix, skip */
                    i+=UTF8CharLenTable[(unsigned char)(*(text+i))];
                } else {
                    founds+=num;
                    i+=UTF8CharLenTable[(unsigned char)(*(text+i))];
                }
            }
            ZVAL_LONG(return_value, founds);
        break;
        case 4:
            array_init(return_value);
            while(i<text_len){
                num = da.commonPrefixSearch((text+i), result_pair, SIZE_OF_RESULT_PAIR, text_len-i);
                if( num == 0) {
                    i+=UTF8CharLenTable[(unsigned char)(*(text+i))];
                } else {
                    while(num>0) {
			char *tkey = (char *)emalloc(sizeof(char)*(result_pair[num-1].length+1));
                        memcpy(tkey, (text+i),result_pair[num-1].length);
                        tkey[result_pair[num-1].length]='\0';
                        if (zend_hash_find(HASH_OF(return_value), tkey, result_pair[num-1].length+1, (void**)&origval) == SUCCESS) {
                            ZVAL_LONG(*origval, Z_LVAL_P(*origval)+1);
                        } else {
                            add_assoc_long(return_value, tkey, 1);
                        }
                        efree(tkey);
                        num--;
                    }
                    i+=UTF8CharLenTable[(unsigned char)(*(text+i))];
                }
            }
        break;
	case 5:
            array_init(return_value);
            while(i<text_len){
                num = da.commonPrefixSearch((text+i), result_pair, SIZE_OF_RESULT_PAIR, text_len-i);

                if( num == 0) {
                    ii=UTF8CharLenTable[(unsigned char)(*(text+i))];

                    if(ii==1&& ( (*(text+i)>='a'&&*(text+i)<='z') ||(*(text+i)>='A'&&*(text+i)<='Z') || (*(text+i)>='0'&&*(text+i)<='9') || *(text+i)=='_' ) ) {
                        while(0==da.commonPrefixSearch((text+i+ii), result_pair, SIZE_OF_RESULT_PAIR, text_len-i-ii) && (1==UTF8CharLenTable[(unsigned char)(*(text+i+ii))]) && ( (*(text+i+ii)>='a'&&*(text+i+ii)<='z') ||(*(text+i+ii)>='A'&&*(text+i+ii)<='Z') || (*(text+i+ii)>='0'&&*(text+i+ii)<='9') || *(text+i+ii)=='_' )  && (i+ii)<text_len ) {
                            ++ii;
                        }
                    	add_next_index_stringl(return_value, (text+i) , ii, 1);
                    }
                    //add_next_index_stringl(return_value, (text+i) , ii, 1);
                    i+=ii;
                } else if(num==1) {
                    add_next_index_stringl(return_value, (text+i) , result_pair[num-1].length, 1);
                    i+=result_pair[num-1].length;
                } else if(num > 1) {

                int temp=getBestWordLength((text+i), (text_len-i));
                    add_next_index_stringl(return_value, (text+i) , temp, 1);
                    i+=temp;

                }

            }


	break;
        case 6:
        {
	        Darts::DoubleArray::Finder finder = da.predictiveSearch(text, text_len);
            array_init(return_value);
        	while (finder.next()) {
                add_next_index_stringl(return_value, (char *)finder.c_str() , finder.length(), 1);
            }
        }
        break;
        default:
            num = da.commonPrefixSearch(text, result_pair, SIZE_OF_RESULT_PAIR, text_len-i);
        	if(num>0) {
		        i=0;
	            array_init(return_value);
		
        		while(i<num) {
		        	MAKE_STD_ZVAL(result_item);
		        	array_init(result_item);
		
		        	add_next_index_long(result_item, result_pair[i].value);
		        	add_next_index_long(result_item, result_pair[i].length);
		        	add_next_index_zval(return_value,result_item);
		        	i++;
		        }

	        } else {
                RETVAL_FALSE;
        	}
    }//end of switch
	da.clear();
}
/* }}} */


/* {{{ 
 */
PHP_FUNCTION(xs_simhash_wide)
{
    zval *array;
    HashTable *arrht;

    char *key;
    uint keylen;
    ulong idx;
    int type;
    zval **ppzval;
    int arrnum;


    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "a", &array) == FAILURE) {
        RETURN_FALSE;
    }

    arrht = HASH_OF(array);
    //arrnum = zend_hash_num_elements(Z_ARRVAL_P(array));
    arrnum=4096;
    if (zend_hash_sort(arrht, zend_qsort, array_key_compare, 0 TSRMLS_CC) == FAILURE) {
        RETURN_FALSE;
    }

        /* Clear histogram */
        int hist[ 64];
        memset( hist, 0, sizeof( hist));

        /* Token position and length */
	
	int stoken_size = arrnum;
        /* Token buffer to create super-tokens */
        uint32_t token_buf[ 2*stoken_size];
        memset( token_buf, 0, sizeof( token_buf));

    for(zend_hash_internal_pointer_reset(arrht); zend_hash_has_more_elements(arrht) == SUCCESS; zend_hash_move_forward(arrht)) {

        type = zend_hash_get_current_key_ex(arrht, &key, &keylen, &idx, 0, NULL);
        if (zend_hash_get_current_data(arrht, (void**)&ppzval) == FAILURE) {
            /* Should never actually fail
             * since the key is known to exist. */
            continue;
        }
        convert_to_string_ex(ppzval);
        /* Output */
        if (type == HASH_KEY_IS_STRING) {
            /* String Key / Associative */

        /* Over all tokens... */

                /* Calculate token hash */
                uint64_t token = get_hashed_token( key, keylen-1);
		//printf( " token: %016llx %s\n", (long long)token, key);

                /* Get token weight */
                int weight = atoi(Z_STRVAL_PP(ppzval));
                //int weight = 1;

                /* Rotate the buffer of tokens */
                if (stoken_size != 1)
                        for (int c=0; c<stoken_size-1; c++) {
                                token_buf[ c*2  ] = token_buf[ c*2+2];
                                token_buf[ c*2+1] = token_buf[ c*2+3];
                        }

                /* Write the new token at the end of the buffer */
                token_buf[ (stoken_size-1)*2  ] = token>>32;
                token_buf[ (stoken_size-1)*2+1] = token&0xffffffff;

                /* Calculate a hash of the super-token */
                uint32_t h1=0x2c759c01; /* Dummy init values */
                uint32_t h2=0xfef136d7;
                hashword2( token_buf, stoken_size*2, &h1, &h2);
                /* Concatenate results to create a super-token */
                uint64_t stoken = ((uint64_t)h1 << 32) +h2;

                //printf( "stoken: %016llx\n", stoken);

                /* Update histogram */
                for (int c=0; c<64; c++)
                        hist[ c] += (stoken & ((uint64_t)1 << c)) == 0 ? -weight : weight;

        } else {
            /* Numeric Key */
        }
    }


        /* Calculate a bit vector from the histogram */
        uint64_t simhash=0;
        for (int c=0; c<64; c++)
                simhash |= (uint64_t)(hist[ c]>=0) << c;

	char simhash_str[16];
	sprintf(simhash_str, "%016llX \n", simhash);
	RETVAL_STRINGL(simhash_str,16 ,1);


}
/* }}} */


PHP_FUNCTION(xs_hdist)
{
    char *shash1_str, *shash2_str;
    int shash1_len, shash2_len;

    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "ss",
                        &shash1_str, &shash1_len, &shash2_str, &shash2_len) == FAILURE) {
        RETURN_FALSE;
    }


	uint64_t shash1, shash2;

	sscanf(shash1_str, "%016llX", &shash1 );
	sscanf(shash2_str, "%016llX", &shash2 );
	

        uint32_t v1 = shash1^shash2;
        uint32_t v2 = (shash1^shash2)>>32;

        v1 = v1 - ((v1>>1) & 0x55555555);
        v2 = v2 - ((v2>>1) & 0x55555555);
        v1 = (v1 & 0x33333333) + ((v1>>2) & 0x33333333);
        v2 = (v2 & 0x33333333) + ((v2>>2) & 0x33333333);
        int c1 = (((v1 + (v1>>4)) & 0xF0F0F0F) * 0x1010101) >> 24;
        int c2 = (((v2 + (v2>>4)) & 0xF0F0F0F) * 0x1010101) >> 24;


	RETVAL_LONG(c1+c2);

}






PHP_FUNCTION(xs_simhash)
{
    zval *array;
    HashTable *arrht;

    char *key;
    uint keylen;
    ulong idx;
    int type;
    zval **ppzval;
    int arrnum;
    zend_bool rawoutput=0;

    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "a|b", &array, &rawoutput) == FAILURE) {
        RETURN_FALSE;
    }

    arrht = HASH_OF(array);
//    if (zend_hash_sort(arrht, zend_qsort, array_key_compare, 0 TSRMLS_CC) == FAILURE) {
//        RETURN_FALSE;
//    }

        /* Clear histogram */
        int hist[ 64];
        memset( hist, 0, sizeof( hist));

        /* Over all tokens... */
        int tokens = 0;
	//char *token_str;
	//int token_len;

    for(zend_hash_internal_pointer_reset(arrht); zend_hash_has_more_elements(arrht) == SUCCESS; zend_hash_move_forward(arrht)) {

        type = zend_hash_get_current_key_ex(arrht, &key, &keylen, &idx, 0, NULL);
        if (zend_hash_get_current_data(arrht, (void**)&ppzval) == FAILURE) {
            /* Should never actually fail
             * since the key is known to exist. */
            continue;
        }
        convert_to_string_ex(ppzval);

                uint64_t token = get_hashed_token( Z_STRVAL_PP(ppzval), Z_STRLEN_PP(ppzval));
                //printf( " token: %016llx %s\n", (long long)token, token_str);

                /* Update histogram, weigth==1 */
                update_hist( hist, token);
                tokens++;



    }
	if(tokens>1)
	        tokens /= 2;

        /* Calculate a bit vector from the histogram */
        uint64_t simhash=0; 
        for (int c=0; c<64; c++)
                simhash |= (uint64_t)((hist[ c]-tokens)>=0) << c;

	if(rawoutput) {
		char simhash_str[8];
		memcpy(simhash_str,&simhash,8);
	        RETVAL_STRINGL(simhash_str,8 ,1);
		
	} else { 
	        char simhash_str[16];
        	sprintf(simhash_str, "%016llX \n", simhash);
	        RETVAL_STRINGL(simhash_str,16 ,1);
	}


}


/* {{{ 
 */
/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */
