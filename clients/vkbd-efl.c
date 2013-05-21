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

#include <linux/input.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "input-method-client-protocol.h"
#include "text-client-protocol.h"

struct vkbd_efl
{
   Ecore_Evas *ee;
   Evas_Object *edje_obj;
   const char *ee_engine;

   struct wl_input_panel *ip;
   struct wl_input_method *im;
   struct wl_input_method_context *im_ctx;

   char *surrounding_text;
   char *preedit_str;
   char *language;

   uint32_t text_direction;
   uint32_t preedit_style;
   uint32_t content_hint;
   uint32_t content_purpose;
   uint32_t serial;
   uint32_t surrounding_cursor;
};

static void
_cb_vkbd_delete_request(Ecore_Evas *ee EINA_UNUSED)
{
   ecore_main_loop_quit();
}

static char *
_vkbd_insert_text(const char *text, uint32_t offset, const char *insert)
{
   char *new_text = malloc(strlen(text) + strlen(insert) + 1);

   printf("%s()\n", __FUNCTION__);

   strncat(new_text, text, offset);
   new_text[offset] = '\0';
   strcat(new_text, insert);
   strcat(new_text, text + offset);

   return new_text;
}

static void
_vkbd_commit_preedit_str(struct vkbd_efl *vkbd)
{
   char *surrounding_text;

   if (!vkbd->preedit_str || !strlen(vkbd->preedit_str) == 0)
      return;

   printf("%s()\n", __FUNCTION__);

   wl_input_method_context_cursor_position(vkbd->im_ctx, 0, 0);
   wl_input_method_context_commit_string(vkbd->im_ctx, vkbd->serial, vkbd->preedit_str);

   if (vkbd->surrounding_text)
     {
        surrounding_text = _vkbd_insert_text(vkbd->surrounding_text, vkbd->surrounding_cursor, vkbd->preedit_str);
        free(vkbd->surrounding_text);
        vkbd->surrounding_text = surrounding_text;
        vkbd->surrounding_cursor += strlen(vkbd->preedit_str);
     }
   else
     {
        vkbd->surrounding_text = strdup(vkbd->preedit_str);
        vkbd->surrounding_cursor = strlen(vkbd->preedit_str);
     }

   free(vkbd->preedit_str);
   vkbd->preedit_str = strdup("");
}

static void
_vkbd_send_preedit_str(struct vkbd_efl *vkbd, int cursor)
{
   unsigned int index = strlen(vkbd->preedit_str);

   printf("%s()\n", __FUNCTION__);

   if (vkbd->preedit_style)
      wl_input_method_context_preedit_styling(vkbd->im_ctx, 0, strlen(vkbd->preedit_str), vkbd->preedit_style);

   if (cursor > 0)
      index = cursor;

   wl_input_method_context_preedit_cursor(vkbd->im_ctx, index);
   wl_input_method_context_preedit_string(vkbd->im_ctx, vkbd->serial, vkbd->preedit_str, vkbd->preedit_str);
}

static void
_vkbd_update_preedit_str(struct vkbd_efl *vkbd, const char *key)
{
   char *tmp;

   printf("%s()\n", __FUNCTION__);

   if (!vkbd->preedit_str)
      vkbd->preedit_str = strdup("");

   tmp = calloc(1, strlen(vkbd->preedit_str) + strlen(key) + 1);
   sprintf(tmp, "%s%s", vkbd->preedit_str, key);
   free(vkbd->preedit_str);
   vkbd->preedit_str = tmp;

   if (strcmp(key, " ") == 0)
      _vkbd_commit_preedit_str(vkbd);
   else
      _vkbd_send_preedit_str(vkbd, -1);
}

static Eina_Bool
_vkbd_is_special_key(const char *key)
{
   static const char *special_keys[] =
     {
        "shift",
        "ABC",
        ".?123",
        ".?12",
        "#+=",
        NULL,
     };
   const char **k;

   for (k = special_keys; *k != NULL; k++)
      if (!strcmp(key, *k))
         return EINA_TRUE;

   return EINA_FALSE;
}

static void
_cb_vkbd_on_key_down(void *data, Evas_Object *obj, const char *emission EINA_UNUSED, const char *source)
{
   struct vkbd_efl *vkbd = data;
   char *src;
   const char *group, *key;

   printf("%s()\n", __FUNCTION__);

   src = strdup(source);
   group = strtok(src, ":");
   key = strtok(NULL, ":");
   if (key == NULL)
       key = ":";

   if (_vkbd_is_special_key(key))
     {
        printf("Ignoring special key '%s'\n", key);
        goto end;
     }
   else if (strcmp(key, "backspace") == 0)
     {
        if (strlen(vkbd->preedit_str) == 0)
             wl_input_method_context_delete_surrounding_text(vkbd->im_ctx, -1, 1);
        else
          {
             vkbd->preedit_str[strlen(vkbd->preedit_str) - 1] = '\0';
             _vkbd_send_preedit_str(vkbd, -1);
          }

        goto end;
     }
   else if (strcmp(key, "enter") == 0)
     {
        _vkbd_commit_preedit_str(vkbd);
        /* wl_input_method_context_keysym(vkbd->im_ctx, vkbd->serial, time, XKB_KEY_Return, key_state, mod_mask); */
        goto end;
     }
   else if (strcmp(key, "space") == 0)
     {
        key = " ";
     }

   printf("KEY = '%s'\n", key);

   _vkbd_update_preedit_str(vkbd, key);

end:
   free(src);
}

static void
_vkbd_im_ctx_surrounding_text(void *data, struct wl_input_method_context *im_ctx, const char *text, uint32_t cursor, uint32_t anchor)
{
   struct vkbd_efl *vkbd = data;

   printf("%s()\n", __FUNCTION__);

   free(vkbd->surrounding_text);
   vkbd->surrounding_text = strdup(text);
   vkbd->surrounding_cursor = cursor;
}

static void
_vkbd_im_ctx_reset(void *data, struct wl_input_method_context *im_ctx)
{
   struct vkbd_efl *vkbd = data;

   printf("%s()\n", __FUNCTION__);

   if (strlen(vkbd->preedit_str))
     {
        free(vkbd->preedit_str);
        vkbd->preedit_str = strdup("");
     }
}

static void
_vkbd_im_ctx_content_type(void *data, struct wl_input_method_context *im_ctx, uint32_t hint, uint32_t purpose)
{
   struct vkbd_efl *vkbd = data;

   printf("%s()\n", __FUNCTION__);

   vkbd->content_hint = hint;
   vkbd->content_purpose = purpose;
}

static void
_vkbd_im_ctx_invoke_action(void *data, struct wl_input_method_context *im_ctx, uint32_t button, uint32_t index)
{
   struct vkbd_efl *vkbd = data;

   printf("%s()\n", __FUNCTION__);

   if (button != BTN_LEFT)
      return;

   _vkbd_send_preedit_str(vkbd, index);
}

static void
_vkbd_im_ctx_commit_state(void *data, struct wl_input_method_context *im_ctx, uint32_t serial)
{
   struct vkbd_efl *vkbd = data;

   printf("%s()\n", __FUNCTION__);

   if (vkbd->surrounding_text)
      fprintf(stderr, "Surrounding text updated: %s\n", vkbd->surrounding_text);

   vkbd->serial = serial;
   /* FIXME */
   wl_input_method_context_language(im_ctx, vkbd->serial, "en");//vkbd->language);
   wl_input_method_context_text_direction(im_ctx, vkbd->serial, WL_TEXT_INPUT_TEXT_DIRECTION_LTR);//vkbd->text_direction);
}

static void
_vkbd_im_ctx_preferred_language(void *data, struct wl_input_method_context *im_ctx, const char *language)
{
   struct vkbd_efl *vkbd = data;

   printf("%s()\n", __FUNCTION__);

   if (language && vkbd->language && !strcmp(language, vkbd->language))
      return;

   if (vkbd->language)
     {
        free(vkbd->language);
        vkbd->language = NULL;
     }

   if (language)
     {
        vkbd->language = strdup(language);
        printf("Language changed, new: '%s\n", language);
     }
}

static const struct wl_input_method_context_listener vkbd_im_context_listener = {
     _vkbd_im_ctx_surrounding_text,
     _vkbd_im_ctx_reset,
     _vkbd_im_ctx_content_type,
     _vkbd_im_ctx_invoke_action,
     _vkbd_im_ctx_commit_state,
     _vkbd_im_ctx_preferred_language,
};

static void
_vkbd_im_activate(void *data, struct wl_input_method *im, struct wl_input_method_context *im_ctx)
{
   struct vkbd_efl *vkbd = data;
   struct wl_array modifiers_map;

   printf("%s()\n", __FUNCTION__);

   if (vkbd->im_ctx)
      wl_input_method_context_destroy(vkbd->im_ctx);

   if (vkbd->preedit_str)
      free(vkbd->preedit_str);

   vkbd->preedit_str = strdup("");
   vkbd->content_hint = 0;
   vkbd->content_purpose = 0;
   free(vkbd->language);
   vkbd->language = NULL;
   free(vkbd->surrounding_text);
   vkbd->surrounding_text = NULL;
   vkbd->serial = 0;

   vkbd->im_ctx = im_ctx;
   wl_input_method_context_add_listener(im_ctx, &vkbd_im_context_listener, vkbd);

   /*
   wl_array_init(&modifiers_map);
   keysym_modifiers_add(&modifiers_map, "Shift");
   keysym_modifiers_add(&modifiers_map, "Control");
   keysym_modifiers_add(&modifiers_map, "Mod1");
   wl_input_method_context_modifiers_map(context, &modifiers_map);
   vkbd->keysym.shift_mask = keysym_modifiers_get_mask(&modifiers_map, "Shift");
   wl_array_release(&modifiers_map);
   */

   /* FIXME */
   wl_input_method_context_language(im_ctx, vkbd->serial, "en");//vkbd->language);
   wl_input_method_context_text_direction(im_ctx, vkbd->serial, WL_TEXT_INPUT_TEXT_DIRECTION_LTR);//vkbd->text_direction);

   evas_object_show(vkbd->edje_obj);
}

static void
_vkbd_im_deactivate(void *data, struct wl_input_method *im, struct wl_input_method_context *im_ctx)
{
   struct vkbd_efl *vkbd = data;

   printf("%s()\n", __FUNCTION__);

   if (!vkbd->im_ctx)
      return;

   wl_input_method_context_destroy(vkbd->im_ctx);
   vkbd->im_ctx = NULL;
   evas_object_hide(vkbd->edje_obj);
}

static const struct wl_input_method_listener vkbd_im_listener = {
     _vkbd_im_activate,
     _vkbd_im_deactivate
};

static Eina_Bool
_vkbd_ui_setup(struct vkbd_efl *vkbd)
{
   static const char path[PATH_MAX] = PKGLIBEXECDIR"/iclone.edj";
   Evas *evas;
   Evas_Coord w, h;

   ecore_evas_alpha_set(vkbd->ee, EINA_TRUE);
   ecore_evas_title_set(vkbd->ee, "EFL virtual keyboard");

   evas = ecore_evas_get(vkbd->ee);
   vkbd->edje_obj = edje_object_add(evas);
   /*ecore_evas_object_associate(vkbd->ee, vkbd->edje_obj, ECORE_EVAS_OBJECT_ASSOCIATE_BASE);*/

   if (!edje_object_file_set(vkbd->edje_obj, path, "main"))
     {
        int err = edje_object_load_error_get(vkbd->edje_obj);
        printf("Error: unable to load the edje file:%s\n", edje_load_error_str(err));
        return EINA_FALSE;
     }

   edje_object_size_min_get(vkbd->edje_obj, &w, &h);
   if (w == 0 || h == 0)
     {
        edje_object_size_min_restricted_calc(vkbd->edje_obj, &w, &h, w, h);
        if (w == 0 || h == 0)
           edje_object_parts_extends_calc(vkbd->edje_obj, NULL, NULL, &w, &h);
     }

   ecore_evas_move_resize(vkbd->ee, 0, 0, w, h);
   evas_object_move(vkbd->edje_obj, 0, 0);
   evas_object_resize(vkbd->edje_obj, w, h);
   evas_object_size_hint_min_set(vkbd->edje_obj, w, h);
   evas_object_size_hint_max_set(vkbd->edje_obj, w, h);

   edje_object_signal_callback_add(vkbd->edje_obj, "key_down", "*", _cb_vkbd_on_key_down, vkbd);
   ecore_evas_callback_delete_request_set(vkbd->ee, _cb_vkbd_delete_request);

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
   struct wl_output *output = NULL;
   struct wl_input_panel_surface *ips;

   printf("%s()\n", __FUNCTION__);

   globals = ecore_wl_globals_get();
   registry = ecore_wl_registry_get();
   wl_list_for_each(global, globals, link)
     {
        if (strcmp(global->interface, "wl_input_panel") == 0)
           vkbd->ip = wl_registry_bind(registry, global->id, &wl_input_panel_interface, 1);
        else if (strcmp(global->interface, "wl_input_method") == 0)
           vkbd->im = wl_registry_bind(registry, global->id, &wl_input_method_interface, 1);
        else if (strcmp(global->interface, "wl_output") == 0)
           output = wl_registry_bind(registry, global->id, &wl_output_interface, 1);
     }

   /* Set input panel surface */
   win = ecore_evas_wayland_window_get(vkbd->ee);
   surface = ecore_wl_window_surface_create(win);
   ips = wl_input_panel_get_input_panel_surface(vkbd->ip, surface);
   wl_input_panel_surface_set_toplevel(ips, output, WL_INPUT_PANEL_SURFACE_POSITION_CENTER_BOTTOM);

   /* Input method listener */
   wl_input_method_add_listener(vkbd->im, &vkbd_im_listener, vkbd);
}

static void
_vkbd_free(struct vkbd_efl *vkbd)
{
   if (vkbd->im_ctx)
      wl_input_method_context_destroy(vkbd->im_ctx);

   evas_object_del(vkbd->edje_obj);
   free(vkbd->preedit_str);
   free(vkbd->surrounding_text);
}

static Eina_Bool
_vkbd_check_evas_engine(struct vkbd_efl *vkbd)
{
   Eina_Bool ret = EINA_FALSE;
   char *env = getenv("ECORE_EVAS_ENGINE");

   if (!env)
     {
        if (ecore_evas_engine_type_supported_get(ECORE_EVAS_ENGINE_WAYLAND_SHM))
           env = "wayland_shm";
        else if (ecore_evas_engine_type_supported_get(ECORE_EVAS_ENGINE_WAYLAND_EGL))
           env = "wayland_egl";
        else
          {
             printf("ERROR: Ecore_Evas does must be compiled with support for Wayland engines\n");
             goto err;
          }
     }
   else if (strcmp(env, "wayland_shm") != 0 && strcmp(env, "wayland_egl") != 0)
     {
        printf("ERROR: ECORE_EVAS_ENGINE must be set to either 'wayland_shm' or 'wayland_egl'\n");
        goto err;
     }

   vkbd->ee_engine = env;
   ret = EINA_TRUE;

err:
   return ret;
}

int
main(int argc EINA_UNUSED, char **argv EINA_UNUSED)
{
   struct vkbd_efl vkbd = {0};
   int ret = EXIT_FAILURE;

   if (!ecore_evas_init())
       return ret;

   if (!edje_init())
       goto ee_err;

   if (!_vkbd_check_evas_engine(&vkbd))
      goto edj_err;

   printf("SELECTED ENGINE = %s\n", vkbd.ee_engine);
   vkbd.ee = ecore_evas_new(vkbd.ee_engine, 0, 0, 1, 1, "frame=0");

   if (!vkbd.ee)
     {
        printf("ERROR: Unable to create Ecore_Evas object\n");
        goto edj_err;
     }

   _vkbd_setup(&vkbd);

   if (!_vkbd_ui_setup(&vkbd))
        goto end;

   ecore_main_loop_begin();

   ret = EXIT_SUCCESS;

   _vkbd_free(&vkbd);

end:
   ecore_evas_free(vkbd.ee);

edj_err:
   edje_shutdown();

ee_err:
   ecore_evas_shutdown();

   return ret;
}
