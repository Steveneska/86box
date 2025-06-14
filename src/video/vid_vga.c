/*
 * 86Box    A hypervisor and IBM PC system emulator that specializes in
 *          running old operating systems and software designed for IBM
 *          PC systems and compatibles from 1981 through fairly recent
 *          system designs based on the PCI bus.
 *
 *          This file is part of the 86Box distribution.
 *
 *          IBM VGA emulation.
 *
 *
 *
 * Authors: Sarah Walker, <https://pcem-emulator.co.uk/>
 *          Miran Grca, <mgrca8@gmail.com>
 *
 *          Copyright 2008-2018 Sarah Walker.
 *          Copyright 2016-2018 Miran Grca.
 */
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <wchar.h>
#include <86box/86box.h>
#include <86box/io.h>
#include <86box/mem.h>
#include <86box/rom.h>
#include <86box/device.h>
#include <86box/timer.h>
#include <86box/video.h>
#include <86box/vid_svga.h>
#include <86box/vid_vga.h>

video_timings_t        timing_vga = { .type = VIDEO_ISA, .write_b = 8, .write_w = 16, .write_l = 32, .read_b = 8, .read_w = 16, .read_l = 32 };

static video_timings_t timing_ps1_svga_isa = { .type = VIDEO_ISA, .write_b = 6, .write_w = 8, .write_l = 16, .read_b = 6, .read_w = 8, .read_l = 16 };
static video_timings_t timing_ps1_svga_mca = { .type = VIDEO_MCA, .write_b = 6, .write_w = 8, .write_l = 16, .read_b = 6, .read_w = 8, .read_l = 16 };

void
vga_out(uint16_t addr, uint8_t val, void *priv)
{
    vga_t  *vga  = (vga_t *) priv;
    svga_t *svga = &vga->svga;
    uint8_t old;

    if (((addr & 0xfff0) == 0x3d0 || (addr & 0xfff0) == 0x3b0) && !(svga->miscout & 1))
        addr ^= 0x60;

    switch (addr) {
        case 0x3D4:
            svga->crtcreg = val & 0x3f;
            return;
        case 0x3D5:
            if (svga->crtcreg & 0x20)
                return;
            if ((svga->crtcreg < 7) && (svga->crtc[0x11] & 0x80))
                return;
            if ((svga->crtcreg == 7) && (svga->crtc[0x11] & 0x80))
                val = (svga->crtc[7] & ~0x10) | (val & 0x10);
            old                       = svga->crtc[svga->crtcreg];
            svga->crtc[svga->crtcreg] = val;
            if (old != val) {
                if (svga->crtcreg < 0xe || svga->crtcreg > 0x10) {
                    if ((svga->crtcreg == 0xc) || (svga->crtcreg == 0xd)) {
                        svga->fullchange = 3;
                        svga->memaddr_latch   = ((svga->crtc[0xc] << 8) | svga->crtc[0xd]) + ((svga->crtc[8] & 0x60) >> 5);
                    } else {
                        svga->fullchange = changeframecount;
                        svga_recalctimings(svga);
                    }
                }
            }
            break;

        default:
            break;
    }
    svga_out(addr, val, svga);
}

uint8_t
vga_in(uint16_t addr, void *priv)
{
    vga_t  *vga  = (vga_t *) priv;
    svga_t *svga = &vga->svga;
    uint8_t temp;

    if (((addr & 0xfff0) == 0x3d0 || (addr & 0xfff0) == 0x3b0) && !(svga->miscout & 1))
        addr ^= 0x60;

    switch (addr) {
        case 0x3D4:
            temp = svga->crtcreg;
            break;
        case 0x3D5:
            if (svga->crtcreg & 0x20)
                temp = 0xff;
            else
                temp = svga->crtc[svga->crtcreg];
            break;
        default:
            temp = svga_in(addr, svga);
            break;
    }
    return temp;
}

void vga_disable(void* p)
{
    vga_t* vga = (vga_t*)p;
    svga_t* svga = &vga->svga;

    io_removehandler(0x03a0, 0x0040, vga_in, NULL, NULL, vga_out, NULL, NULL, vga);
    mem_mapping_disable(&svga->mapping);
    svga->vga_enabled = 0;
}

void vga_enable(void* p)
{
    vga_t* vga = (vga_t*)p;
    svga_t* svga = &vga->svga;

    io_sethandler(0x03c0, 0x0020, vga_in, NULL, NULL, vga_out, NULL, NULL, vga);
    if (!(svga->miscout & 1))
        io_sethandler(0x03a0, 0x0020, vga_in, NULL, NULL, vga_out, NULL, NULL, vga);

    mem_mapping_enable(&svga->mapping);
    svga->vga_enabled = 1;
}

int vga_isenabled(void* p)
{
    vga_t* vga = (vga_t*)p;
    svga_t* svga = &vga->svga;

    return svga->vga_enabled;
}

void
vga_init(const device_t *info, vga_t *vga, int enabled)
{
    svga_init(info, &vga->svga, vga, 1 << 18, /*256kb*/
              NULL,
              vga_in, vga_out,
              NULL,
              NULL);

    vga->svga.bpp     = 8;
    vga->svga.miscout = 1;

    vga->svga.vga_enabled = enabled;
}

static void *
vga_standalone_init(const device_t *info)
{
    vga_t *vga = calloc(1, sizeof(vga_t));

    rom_init(&vga->bios_rom, "roms/video/vga/ibm_vga.bin", 0xc0000, 0x8000, 0x7fff, 0x2000, MEM_MAPPING_EXTERNAL);

    video_inform(VIDEO_FLAG_TYPE_SPECIAL, &timing_vga);

    vga_init(info, vga, 0);

    io_sethandler(0x03c0, 0x0020, vga_in, NULL, NULL, vga_out, NULL, NULL, vga);
 
    return vga;
}

/*PS/1 uses a standard VGA controller, but with no option ROM*/
void *
ps1vga_init(const device_t *info)
{
    vga_t *vga = calloc(1, sizeof(vga_t));

    if (info->flags & DEVICE_MCA)
        video_inform(VIDEO_FLAG_TYPE_SPECIAL, &timing_ps1_svga_mca);
    else
        video_inform(VIDEO_FLAG_TYPE_SPECIAL, &timing_ps1_svga_isa);

    vga_init(info, vga, 1);

    io_sethandler(0x03c0, 0x0020, vga_in, NULL, NULL, vga_out, NULL, NULL, vga);

    return vga;
}

static int
vga_available(void)
{
    return rom_present("roms/video/vga/ibm_vga.bin");
}

void
vga_close(void *priv)
{
    vga_t *vga = (vga_t *) priv;

    svga_close(&vga->svga);

    free(vga);
}

void
vga_speed_changed(void *priv)
{
    vga_t *vga = (vga_t *) priv;

    svga_recalctimings(&vga->svga);
}

void
vga_force_redraw(void *priv)
{
    vga_t *vga = (vga_t *) priv;

    vga->svga.fullchange = changeframecount;
}

const device_t vga_device = {
    .name          = "IBM VGA",
    .internal_name = "vga",
    .flags         = DEVICE_ISA,
    .local         = 0,
    .init          = vga_standalone_init,
    .close         = vga_close,
    .reset         = NULL,
    .available     = vga_available,
    .speed_changed = vga_speed_changed,
    .force_redraw  = vga_force_redraw,
    .config        = NULL
};

const device_t ps1vga_device = {
    .name          = "IBM PS/1 VGA",
    .internal_name = "ps1vga",
    .flags         = DEVICE_ISA,
    .local         = 0,
    .init          = ps1vga_init,
    .close         = vga_close,
    .reset         = NULL,
    .available     = NULL,
    .speed_changed = vga_speed_changed,
    .force_redraw  = vga_force_redraw,
    .config        = NULL
};

const device_t ps1vga_mca_device = {
    .name          = "IBM PS/1 VGA",
    .internal_name = "ps1vga_mca",
    .flags         = DEVICE_MCA,
    .local         = 0,
    .init          = ps1vga_init,
    .close         = vga_close,
    .reset         = NULL,
    .available     = NULL,
    .speed_changed = vga_speed_changed,
    .force_redraw  = vga_force_redraw,
    .config        = NULL
};
