/* camlib.h */
/* Created by Enomoto Sanshiro on 23 July 1999. */
/* Last updated by Enomoto Sanshiro on 18 September 2002. */


#ifndef __CAMLIB_H_INCLUDED
#define __CAMLIB_H_INCLUDED 1


#ifdef __cplusplus
extern "C" {
#endif

int COPEN(void);
int CCLOSE(void);
int CSETCR(int crate_number);
int CGENZ(void);
int CGENC(void);
int CSETI(void);
int CREMI(void);
int CAMAC(int naf, int *data, int *q, int *x);
int CELAM(int mask);
int CDLAM(void);
int CWLAM(int timeout);

#ifdef __cplusplus
}
#endif


#define CAM_Open COPEN
#define CAMOPN COPEN
#define CAM_Close CCLOSE
#define CAMCLS CCLOSE
#define CAMACL CAMAC
#define CAMACW CAMAC
#define CAM_EnableLAM CELAM
#define CENLAM CELAM
#define CAM_DisableLAM CDLAM
#define CDSLAM CDLAM
#define CAM_WaitLAM CWLAM
#define CWTLAM CWLAM

#define NAF(n, a, f) ((((n) << 9) | ((a) << 5) | (f)) & 0x00003fff)


#endif
