// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2018,2019 IBM Corp.

#include "ilpc.h"
#include "log.h"
#include "rev.h"

#include <errno.h>
#include <stdio.h>
#include <stdint.h>

#define LPC_HICRB_ILPCB_RO (1 << 6)

int ilpcb_init(struct ilpcb *ctx)
{
    return sio_init(&ctx->sio);
}

int ilpcb_destroy(struct ilpcb *ctx)
{
    return sio_destroy(&ctx->sio);
}

int ilpcb_probe(struct ilpcb *ctx)
{
    struct ahb ahb;
    int rc;

    logd("Probing %s\n", ahb_interface_names[ahb_ilpcb]);

    if (!sio_present(&ctx->sio))
        return 0;

    rc = rev_probe(ahb_use(&ahb, ahb_ilpcb, ctx));

    return rc < 0 ? rc : 1;
}

int ilpcb_mode(struct ilpcb *ctx)
{
    uint32_t hicrb = 0;
    int rc;

    rc = ilpcb_readl(ctx, 0x1e789100, &hicrb);
    if (rc)
        return rc;

    return !!(hicrb & LPC_HICRB_ILPCB_RO); /* Maps to enum ilpcb_mode */
}

int ilpcb_read(struct ilpcb *ctx, size_t addr, void *buf, size_t len)
{
    struct sio *sio = &ctx->sio;
    size_t remaining;
    uint8_t data;
    int locked;
    int rc;

    rc = sio_unlock(sio);
    if (rc)
        goto done;

    /* Select iLPC2AHB */
    rc = sio_select(sio, sio_ilpc);
    if (rc)
        goto done;

    /* Enable iLPC2AHB */
    rc = sio_writeb(sio, 0x30, 0x01);
    if (rc)
        goto done;

    /* 1-byte access */
    rc = sio_writeb(sio, 0xf8, 0);
    if (rc)
        goto done;

    /* XXX: Think about optimising this */
    remaining = len;
    while (remaining) {
        /* Address */
        rc |= sio_writeb(sio, 0xf0, addr >> 24);
        rc |= sio_writeb(sio, 0xf1, addr >> 16);
        rc |= sio_writeb(sio, 0xf2, addr >>  8);
        rc |= sio_writeb(sio, 0xf3, addr      );
        if (rc)
            goto done;

        /* Trigger */
        rc = sio_readb(sio, 0xfe, &data);
        if (rc)
            goto done;

        rc = sio_readb(sio, 0xf7, (uint8_t *)buf);
        if (rc)
            goto done;

        buf++;
        remaining--;
    }

done:
    locked = sio_lock(sio);
    if (locked) {
        errno = -locked;
        perror("sio_lock");
    }

    return rc ? rc : len;
}

int ilpcb_write(struct ilpcb *ctx, size_t addr, const void *buf, size_t len)
{
    struct sio *sio = &ctx->sio;
    size_t remaining;
    int locked;
    int rc;

    rc = sio_unlock(sio);
    if (rc)
        goto done;

    /* Select iLPC2AHB */
    rc = sio_select(sio, sio_ilpc);
    if (rc)
        goto done;

    /* Enable iLPC2AHB */
    rc = sio_writeb(sio, 0x30, 0x01);
    if (rc)
        goto done;

    /* 1-byte access */
    rc = sio_writeb(sio, 0xf8, 0);
    if (rc)
        goto done;

    /* XXX: Think about optimising this */
    remaining = len;
    while (remaining) {
        /* Address */
        rc |= sio_writeb(sio, 0xf0, addr >> 24);
        rc |= sio_writeb(sio, 0xf1, addr >> 16);
        rc |= sio_writeb(sio, 0xf2, addr >>  8);
        rc |= sio_writeb(sio, 0xf3, addr      );
        if (rc)
            goto done;

        rc = sio_writeb(sio, 0xf7, *(uint8_t *)buf);
        if (rc)
            goto done;

        /* Trigger */
        rc = sio_writeb(sio, 0xfe, 0xcf);
        if (rc)
            goto done;

        buf++;
        remaining--;
    }

done:
    locked = sio_lock(sio);
    if (locked) {
        errno = -locked;
        perror("sio_lock");
    }

    return rc ? rc : len;
}

/* Little-endian */
int ilpcb_readl(struct ilpcb *ctx, size_t addr, uint32_t *val)
{
    struct sio *sio = &ctx->sio;
    uint32_t extracted;
    uint8_t data;
    int locked;
    int rc;

    rc = sio_unlock(sio);
    if (rc)
        goto done;

    /* Select iLPC2AHB */
    rc = sio_select(sio, sio_ilpc);
    if (rc)
        goto done;

    /* Enable iLPC2AHB */
    rc = sio_writeb(sio, 0x30, 0x01);
    if (rc)
        goto done;

    /* 4-byte access */
    rc = sio_writeb(sio, 0xf8, 2);
    if (rc)
        goto done;

    /* Address */
    rc |= sio_writeb(sio, 0xf0, addr >> 24);
    rc |= sio_writeb(sio, 0xf1, addr >> 16);
    rc |= sio_writeb(sio, 0xf2, addr >>  8);
    rc |= sio_writeb(sio, 0xf3, addr      );
    if (rc)
        goto done;

    /* Trigger */
    rc = sio_readb(sio, 0xfe, &data);
    if (rc)
        goto done;

    /* Value */
    extracted = 0;
    rc |= sio_readb(sio, 0xf4, &data);
    extracted = (extracted << 8) | data;
    rc |= sio_readb(sio, 0xf5, &data);
    extracted = (extracted << 8) | data;
    rc |= sio_readb(sio, 0xf6, &data);
    extracted = (extracted << 8) | data;
    rc |= sio_readb(sio, 0xf7, &data);
    extracted = (extracted << 8) | data;
    if (rc)
        goto done;

    *val = extracted;

done:
    locked = sio_lock(sio);
    if (locked) {
        errno = -locked;
        perror("sio_lock");
    }

    return rc;
}

/* Little-endian */
int ilpcb_writel(struct ilpcb *ctx, size_t addr, uint32_t val)
{
    struct sio *sio = &ctx->sio;
    int locked;
    int rc;

    rc = sio_unlock(sio);
    if (rc)
        goto done;

    /* Select iLPC2AHB */
    rc = sio_select(sio, sio_ilpc);
    if (rc)
        goto done;

    /* Enable iLPC2AHB */
    rc = sio_writeb(sio, 0x30, 0x01);
    if (rc)
        goto done;

    /* 4-byte access */
    rc = sio_writeb(sio, 0xf8, 2);
    if (rc)
        goto done;

    /* Address */
    rc |= sio_writeb(sio, 0xf0, addr >> 24);
    rc |= sio_writeb(sio, 0xf1, addr >> 16);
    rc |= sio_writeb(sio, 0xf2, addr >>  8);
    rc |= sio_writeb(sio, 0xf3, addr >>  0);
    if (rc)
        goto done;

    /* Value */
    rc |= sio_writeb(sio, 0xf4, val >> 24);
    rc |= sio_writeb(sio, 0xf5, val >> 16);
    rc |= sio_writeb(sio, 0xf6, val >>  8);
    rc |= sio_writeb(sio, 0xf7, val >>  0);
    if (rc)
        goto done;

    /* Trigger */
    rc = sio_writeb(sio, 0xfe, 0xcf);
    if (rc)
        goto done;

done:
    locked = sio_lock(sio);
    if (locked) {
        errno = -locked;
        perror("Failed to lock SuperIO device");
    }

    return rc;
}
