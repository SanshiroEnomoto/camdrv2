/* toyocamac.h */
/* Created by Sanshiro Enomoto on 23 July 1999. */
/* Last updated by Sanshiro Enomoto on 18 September 2002. */


#ifndef __TOYOCAMAC_H__
#define __TOYOCAMAC_H__


#ifdef __cplusplus
extern "C" {
#endif

void setcn(unsigned crate_number);
unsigned getcn(void);
void execz(void);
void execc(void);
void seti(void);
void clri(void);
void setei(void);
void clrei(void);
unsigned long rlam(void);
unsigned int camac_0(unsigned n, unsigned a, unsigned f);
unsigned int camac_16(unsigned n, unsigned a, unsigned f, unsigned *data);
unsigned int camac_24(unsigned n, unsigned a, unsigned f, unsigned *data);
unsigned int camac_16w(unsigned n, unsigned a, unsigned f, unsigned data);
unsigned int camac_24w(unsigned n, unsigned a, unsigned f, unsigned data);

#ifdef __cplusplus
}
#endif


#define SetCN setcn
#define GetCN getcn
#define ExecZ execz
#define ExecC execc
#define SetI seti
#define ClrI clri
#define SetEI setei
#define ClrEI clrei
#define RLAM rlam
#define Camac0 camac_0
#define Camac16 camac_16
#define Camac24 camac_24
#define Camac16w camac_16w
#define Camac24w camac_24w


#endif
