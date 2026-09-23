#ifndef PARAMS_H
#define PARAMS_H

#include "../inc/kernel_msgpool.h"

int params_init(struct msgpool_ctx *ctx);
void params_cleanup(void);

#endif /* PARAMS_H */