/*
 * Copyright © 2013 Intel Corporation
 *
 * Permission to use, copy, modify, distribute, and sell this software and
 * its documentation for any purpose is hereby granted without fee, provided
 * that the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation, and that the name of the copyright holders not be used in
 * advertising or publicity pertaining to distribution of the software
 * without specific, written prior permission.  The copyright holders make
 * no representations about the suitability of this software for any
 * purpose.  It is provided "as is" without express or implied warranty.
 *
 * THE COPYRIGHT HOLDERS DISCLAIM ALL WARRANTIES WITH REGARD TO THIS
 * SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND
 * FITNESS, IN NO EVENT SHALL THE COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER
 * RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF
 * CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#else
#define EINA_UNUSED __UNUSED__
#endif

#include <Eina.h>
#include <Ecore.h>
#include <Ecore_Wayland.h>
#include <Ecore_Evas.h>
#include <Edje.h>
#include <stdio.h>

#include "input-method-client-protocol.h"
#include "text-client-protocol.h"

struct vkbd_efl
{
   Ecore_Evas *ee;
   struct input_panel *ipanel;
   struct input_method *imethod;
};

static void
_vkbd_on_delete(Ecore_Evas *ee EINA_UNUSED)
{
   ecore_main_loop_quit();
}

static Eina_Bool
_vkbd_ui_setup(struct vkbd_efl *vkbd)
{
   static const char path[PATH_MAX] = PKGLIBEXECDIR"/iclone.edj";
   Evas *evas;
   Evas_Object *edje_obj;
   Evas_Coord w, h;

   ecore_evas_alpha_set(vkbd->ee, EINA_TRUE);
   ecore_evas_title_set(vkbd->ee, "EFL virtual keyboard");

   evas = ecore_evas_get(vkbd->ee);
   edje_obj = edje_object_add(evas);
   ecore_evas_object_associate(vkbd->ee, edje_obj, ECORE_EVAS_OBJECT_ASSOCIATE_BASE);

   if (!edje_object_file_set(edje_obj, path, "main"))
     {
        int err = edje_object_load_error_get(edje_obj);
        fprintf(stderr, "error loading the edje file:%s\n", edje_load_error_str(err));
        return EINA_FALSE;
     }

   edje_object_size_min_get(edje_obj, &w, &h);
   if (w == 0 || h == 0)
     {
        edje_object_size_min_restricted_calc(edje_obj, &w, &h, w, h);
        if (w == 0 || h == 0)
           edje_object_parts_extends_calc(edje_obj, NULL, NULL, &w, &h);
     }

   evas_object_resize(edje_obj, w, h);
   evas_object_size_hint_min_set(edje_obj, w, h);
   evas_object_size_hint_max_set(edje_obj, w, h);
   evas_object_move(edje_obj, 0, 0);
   evas_object_show(edje_obj);

   ecore_evas_callback_delete_request_set(vkbd->ee, _vkbd_on_delete);

   ecore_evas_show(vkbd->ee);

   return EINA_TRUE;
}

static void
_vkbd_setup(struct vkbd_efl *vkbd)
{
   struct wl_list *globals;
   struct wl_registry *registry;
   Ecore_Wl_Global *global;

   Ecore_Wl_Window *win;
   struct wl_surface *surface;
   struct input_panel_surface *ips;

   globals = ecore_wl_globals_get();
   registry = ecore_wl_registry_get();
   wl_list_for_each(global, globals, link)
     {
        if (strcmp(global->interface, "input_panel") == 0)
           vkbd->ipanel = wl_registry_bind(registry, global->id, &input_panel_interface, 1);
        else if (strcmp(global->interface, "input_method") == 0)
           vkbd->imethod = wl_registry_bind(registry, global->id, &input_method_interface, 1);
     }

   win = ecore_evas_wayland_window_get(vkbd->ee);
   surface = ecore_wl_window_surface_create(win);
   ips = input_panel_get_input_panel_surface(vkbd->ipanel, surface);
   input_panel_surface_set_toplevel(ips, INPUT_PANEL_SURFACE_POSITION_CENTER_BOTTOM);
}

int
main(int argc EINA_UNUSED, char **argv EINA_UNUSED)
{
   struct vkbd_efl vkbd = {0};
   int ret = EXIT_FAILURE;

   if (!ecore_evas_init()) return ret;
   if (!edje_init()) goto ee_err;

   vkbd.ee = ecore_evas_new(NULL, 0, 0, 1, 1, "frame=0");

   if (!vkbd.ee) goto edj_err;

   _vkbd_setup(&vkbd);
   if (!_vkbd_ui_setup(&vkbd)) goto end;

   ecore_main_loop_begin();

   ret = EXIT_SUCCESS;

end:
   ecore_evas_free(vkbd.ee);

edj_err:
   edje_shutdown();

ee_err:
   ecore_evas_shutdown();

   return ret;
}
