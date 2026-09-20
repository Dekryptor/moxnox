#ifndef _CRYPT_H
#define _CRYPT_H

#ifdef __cplusplus
extern "C" {
#endif

struct crypt_data {
	int initialized;
	char __buf[256];
};

char *crypt(const char *, const char *);
char *crypt_r(const char *, const char *, struct crypt_data *);
char *__crypt_r(const char *, const char *, struct crypt_data *);
char *__crypt_des(const char *, const char *, char *);
char *__crypt_md5(const char *, const char *, char *);
char *__crypt_blowfish(const char *, const char *, char *);
char *__crypt_sha256(const char *, const char *, char *);
char *__crypt_sha512(const char *, const char *, char *);

#define weak_alias(a, b)
#define hidden

#ifdef __cplusplus
}
#endif

#endif
