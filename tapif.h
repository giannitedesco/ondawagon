#ifndef _TAPIF_H
#define _TAPIF_H

typedef struct _tapif *tapif_t;

tapif_t tapif_open(const char *ifname);
void tapif_close(tapif_t t);

#endif /* _TAPIF_H */
