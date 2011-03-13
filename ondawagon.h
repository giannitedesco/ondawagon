#ifndef _DEVILIST_H
#define _ONDAWAGON_H

typedef struct _dongle *dongle_t;

extern const char *odw_cmd;

int dongle_list_all(dongle_t *dev, size_t *nmemb);
dongle_t dongle_open(const char *serial);

#endif /* _ONDAWAGON_H */
