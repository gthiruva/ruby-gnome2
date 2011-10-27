/* -*- c-file-style: "ruby"; indent-tabs-mode: nil -*- */
/*
 *  Copyright (C) 2011  Ruby-GNOME2 Project Team
 *  Copyright (C) 2002-2004 Masao Mutoh
 *  Copyright (C) 2000  Yasushi Shoji
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2.1 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 *  MA  02110-1301  USA
 */

#include "rbgdk-pixbuf.h"
#include <string.h>

#ifdef HAVE_GDK_PIXBUF_GDK_PIXBUF_IO_H
#define GDK_PIXBUF_ENABLE_BACKEND
#include <gdk-pixbuf/gdk-pixbuf-io.h>
#endif

#define RG_TARGET_NAMESPACE cPixbuf
#define _SELF(s) GDK_PIXBUF(RVAL2GOBJ(s)) 

#define NOMEM_ERROR(error) g_set_error(error,\
                             GDK_PIXBUF_ERROR,\
                             GDK_PIXBUF_ERROR_INSUFFICIENT_MEMORY,\
                             "Insufficient memory to load image file");

static ID id_pixdata;

/****************************************************/
/* The GdkPixbuf Structure */
static int
pixels_size(GdkPixbuf *pixbuf)
{
    int height, width, rowstride, n_channels, bits_per_sample;

    height = gdk_pixbuf_get_height(pixbuf);
    width = gdk_pixbuf_get_width(pixbuf);
    rowstride = gdk_pixbuf_get_rowstride(pixbuf);
    n_channels = gdk_pixbuf_get_n_channels(pixbuf);
    bits_per_sample = gdk_pixbuf_get_bits_per_sample(pixbuf);

    return ((height - 1) * rowstride +
            width * ((n_channels * bits_per_sample + 7) / 8));
}

static VALUE
get_pixels(VALUE self)
{
    GdkPixbuf *pixbuf = _SELF(self);
    int size;

    size = pixels_size(pixbuf);
    return rb_str_new((const char *)gdk_pixbuf_get_pixels(pixbuf), size);
}

static VALUE
set_pixels(VALUE self, VALUE pixels)
{
    GdkPixbuf *pixbuf = _SELF(self);
    int size;
    int arg_size;

    size = pixels_size(pixbuf);

    StringValue(pixels);
    arg_size = RSTRING_LEN(pixels);
    if (arg_size != size)
      rb_raise(rb_eRangeError,
               "Pixels are %i bytes, %i bytes supplied.",
               size, arg_size);

    /* The user currently cannot get a pointer to the actual
     * pixels, the data is copied to a String. */
    memcpy(gdk_pixbuf_get_pixels(pixbuf),
           RSTRING_PTR(pixels), MIN(RSTRING_LEN(pixels), size));

    return pixels;
}

static VALUE
get_option(VALUE self, VALUE key)
{
    const gchar* ret = gdk_pixbuf_get_option(_SELF(self), RVAL2CSTR(key));
    return ret ? CSTR2RVAL(ret) : Qnil;
}

/****************************************************/
/* File opening */
/* Image Data in Memory */
static VALUE
initialize(int argc, VALUE *argv, VALUE self)
{
    GdkPixbuf* buf;
    GError* error = NULL;
    VALUE arg1, arg2, arg3, arg4, arg5, arg6, arg7;

    rb_scan_args(argc, argv, "16", &arg1, &arg2, &arg3, &arg4, &arg5, &arg6, &arg7);

    if (argc == 7){
        buf = gdk_pixbuf_new_from_data((const guchar*)RVAL2CSTR(arg1), 
                                       RVAL2GENUM(arg2, GDK_TYPE_COLORSPACE),
                                       RVAL2CBOOL(arg3),   NUM2INT(arg4),
                                       NUM2INT(arg5), NUM2INT(arg6),
                                       NUM2INT(arg7), NULL, NULL);
        if (buf == NULL){
            rb_gc();
            buf = gdk_pixbuf_new_from_data((const guchar*)RVAL2CSTR(arg1), 
                                           RVAL2GENUM(arg2, GDK_TYPE_COLORSPACE),
                                           RVAL2CBOOL(arg3),   NUM2INT(arg4),
                                           NUM2INT(arg5), NUM2INT(arg6),
                                           NUM2INT(arg7), NULL, NULL);
            if (buf == NULL) NOMEM_ERROR(&error);
        }
        // Save a reference to the string because the pixbuf doesn't copy it.
        G_RELATIVE(self, arg1);
    } else if (argc == 5){
        if (rb_obj_is_kind_of(arg1, GTYPE2CLASS(GDK_TYPE_PIXBUF))){
            buf = gdk_pixbuf_new_subpixbuf(_SELF(arg1), 
                                           NUM2INT(arg2), NUM2INT(arg3), 
                                           NUM2INT(arg4), NUM2INT(arg5));
            if (buf == NULL){
                rb_gc();
                buf = gdk_pixbuf_new_subpixbuf(_SELF(arg1), 
                                               NUM2INT(arg2), NUM2INT(arg3), 
                                               NUM2INT(arg4), NUM2INT(arg5));
                if (buf == NULL) NOMEM_ERROR(&error);
            }
        } else if (rb_obj_is_kind_of(arg1, GTYPE2CLASS(GDK_TYPE_COLORSPACE))){
            buf = gdk_pixbuf_new(RVAL2GENUM(arg1, GDK_TYPE_COLORSPACE),
                                 RVAL2CBOOL(arg2), NUM2INT(arg3),
                                 NUM2INT(arg4), NUM2INT(arg5));
            if (buf == NULL){
                rb_gc();
                buf = gdk_pixbuf_new(RVAL2GENUM(arg1, GDK_TYPE_COLORSPACE),
                                     RVAL2CBOOL(arg2), NUM2INT(arg3),
                                     NUM2INT(arg4), NUM2INT(arg5));
                if (buf == NULL) NOMEM_ERROR(&error);
            }
        } else {
            rb_raise(rb_eArgError, "Wrong type of 1st argument or wrong number of arguments");
        }
    } else if (argc == 4) {
#if RBGDK_PIXBUF_CHECK_VERSION(2,6,0)
        int width = NUM2INT(arg2);
        int height = NUM2INT(arg3);
#if ! RBGDK_PIXBUF_CHECK_VERSION(2,8,0)
        if (width < 0 || height < 0)
            rb_warning("For scaling on load, a negative value for width or height are not supported in GTK+ < 2.8.0");
#endif
        buf = gdk_pixbuf_new_from_file_at_scale(RVAL2CSTR(arg1),
                                                width, height,
                                                RVAL2CBOOL(arg4), &error);
        if (buf == NULL){
            rb_gc();
            error = NULL;
            buf = gdk_pixbuf_new_from_file_at_scale(RVAL2CSTR(arg1),
                                                    NUM2INT(arg2), NUM2INT(arg3), 
                                                    RVAL2CBOOL(arg4), &error);
        }
#else
        rb_warning("Scaling on load not supported in GTK+ < 2.6.0");
        buf = gdk_pixbuf_new_from_file(RVAL2CSTR(arg1), &error);
        if (buf == NULL){
            error = NULL;
            rb_gc();
            buf = gdk_pixbuf_new_from_file(RVAL2CSTR(arg1), &error);
        }
#endif
    } else if (argc == 3) {
#if RBGDK_PIXBUF_CHECK_VERSION(2,4,0)
        buf = gdk_pixbuf_new_from_file_at_size(RVAL2CSTR(arg1),
                                               NUM2INT(arg2), NUM2INT(arg3), &error);
        if (buf == NULL){
            rb_gc();
            error = NULL;
            buf = gdk_pixbuf_new_from_file_at_size(RVAL2CSTR(arg1),
                                                   NUM2INT(arg2), NUM2INT(arg3), &error);
        }
#else
        rb_warning("Sizing on load not supported in GTK+ < 2.4.0");
        buf = gdk_pixbuf_new_from_file(RVAL2CSTR(arg1), &error);
        if (buf == NULL){
            error = NULL;
            rb_gc();
            buf = gdk_pixbuf_new_from_file(RVAL2CSTR(arg1), &error);
        }
#endif
    } else if (argc == 2) {
        /* TODO: Is this really up to the caller to decide? */
        gboolean copy_pixels = RVAL2CBOOL(arg2);
        long n;
        guint8 *data = RVAL2GUINT8S(arg1, n);
        buf = gdk_pixbuf_new_from_inline(n, data, copy_pixels, &error);
        if (buf == NULL) {
            rb_gc();
            error = NULL;
            buf = gdk_pixbuf_new_from_inline(n, data, copy_pixels, &error);
        }
        /* need to manage the returned value */
        rb_ivar_set(self, id_pixdata, Data_Wrap_Struct(rb_cData, NULL, g_free, data));
    } else if (argc == 1){
        if (TYPE(arg1) == T_STRING) {
            buf = gdk_pixbuf_new_from_file(RVAL2CSTR(arg1), &error);
            if (buf == NULL){
                rb_gc();
                error = NULL;
                buf = gdk_pixbuf_new_from_file(RVAL2CSTR(arg1), &error);
            }
        } else if (TYPE(arg1) == T_ARRAY) {
            const gchar **data = RVAL2STRV(arg1);
            buf = gdk_pixbuf_new_from_xpm_data(data);
            if (buf == NULL) {
                rb_gc();
                buf = gdk_pixbuf_new_from_xpm_data(data);
            }
            g_free(data);
            if (buf == NULL)
                NOMEM_ERROR(&error);
        } else {
            rb_raise(rb_eArgError, "Wrong type of 1st argument or wrong number of arguments");
        }
    } else {
        rb_raise(rb_eArgError, "Wrong number of arguments");
    }

    if (error || ! buf) RAISE_GERROR(error);
    
    G_INITIALIZE(self, buf);
    return Qnil;
}

static VALUE
copy(VALUE self)
{
    VALUE ret;
    GdkPixbuf* dest = gdk_pixbuf_copy(_SELF(self));
    if (dest == NULL)
        return Qnil;
    ret = GOBJ2RVAL(dest);
    g_object_unref(dest);
    return ret;
}

#if RBGDK_PIXBUF_CHECK_VERSION(2,4,0)
static VALUE
get_file_info(G_GNUC_UNUSED VALUE self, VALUE filename)
{
    gint width, height;

    GdkPixbufFormat* format = gdk_pixbuf_get_file_info(RVAL2CSTR(filename), 
                                                       &width, &height);
    return format ? rb_ary_new3(3, BOXED2RVAL(format, GDK_TYPE_PIXBUF_FORMAT), INT2NUM(width), INT2NUM(height)) : Qnil;
}

#endif

static VALUE
save_to(VALUE self, const gchar *filename, const gchar *type, VALUE options)
{
    VALUE result = self;
    GError *error = NULL;
    gchar **keys = NULL;
    gchar **values = NULL;

    if (!NIL_P(options)) {
        VALUE ary, key, value;
        ID to_s;
        gint len, i;

        Check_Type(options, T_HASH);
        to_s = rb_intern("to_s");

        ary = rb_funcall(options, rb_intern("to_a"), 0);
        len = RARRAY_LEN(ary);
        keys = ALLOCA_N(gchar *, len + 1);
        values = ALLOCA_N(gchar *, len + 1);
        for (i = 0; i < len; i++) {
            key = RARRAY_PTR(RARRAY_PTR(ary)[i])[0];
            if (SYMBOL_P(key)) {
                const char *const_key;
                const_key = rb_id2name(SYM2ID(key));
                keys[i] = (gchar *)const_key;
            } else {
                keys[i] = (gchar *)RVAL2CSTR(key);
            }
            value = rb_funcall(RARRAY_PTR(RARRAY_PTR(ary)[i])[1], to_s, 0);
            values[i] = (gchar *)RVAL2CSTR(value);
        }
        keys[len] = NULL;
        values[len] = NULL;
    }

    if (filename) {
        gdk_pixbuf_savev(_SELF(self), filename, type, keys, values, &error);
    }
#if RBGDK_PIXBUF_CHECK_VERSION(2,4,0)
    else {
        gchar *buffer;
        gsize buffer_size;
        if (gdk_pixbuf_save_to_bufferv(_SELF(self), &buffer, &buffer_size,
                                       type, keys, values, &error))
            result = rb_str_new(buffer, buffer_size);
    }
#endif

    if (error)
        RAISE_GERROR(error);

    return result;
}

/****************************************************/
/* File saving */
static VALUE
save(int argc, VALUE *argv, VALUE self)
{
    VALUE filename, type, options;

    rb_scan_args(argc, argv, "21", &filename, &type, &options);

    return save_to(self, RVAL2CSTR(filename), RVAL2CSTR(type), options);
}

#if RBGDK_PIXBUF_CHECK_VERSION(2,4,0)
/* XXX
gboolean    gdk_pixbuf_save_to_callbackv    (GdkPixbuf *pixbuf,
                                             GdkPixbufSaveFunc save_func,
                                             gpointer user_data,
                                             const char *type,
                                             char **option_keys,
                                             char **option_values,
                                             GError **error);
*/

static VALUE
save_to_buffer(int argc, VALUE *argv, VALUE self)
{
    VALUE type, options;

    rb_scan_args(argc, argv, "11", &type, &options);

    return save_to(self, NULL, RVAL2CSTR(type), options);
}
#endif

/****************************************************/
/* Scaling */
static VALUE
scale_simple(int argc, VALUE *argv, VALUE self)
{
    GdkPixbuf* dest;
    VALUE dest_width, dest_height, interp_type, ret;
    GdkInterpType type = GDK_INTERP_BILINEAR;

    rb_scan_args(argc, argv, "21", &dest_width, &dest_height,
                 &interp_type);

    if (!NIL_P(interp_type))
        type = RVAL2GENUM(interp_type, GDK_TYPE_INTERP_TYPE);
    
    dest = gdk_pixbuf_scale_simple(_SELF(self),
                                   NUM2INT(dest_width),
                                   NUM2INT(dest_height),
                                   type);
    if (dest == NULL)
        return Qnil;

    ret = GOBJ2RVAL(dest);
    g_object_unref(dest);
    return ret;
}

static VALUE
scale(int argc, VALUE *argv, VALUE self)
{
    GdkInterpType type = GDK_INTERP_BILINEAR;

    VALUE src, src_x, src_y, src_width, src_height; 
    VALUE offset_x, offset_y, scale_x, scale_y, interp_type;

    rb_scan_args(argc, argv, "91", &src, &src_x, &src_y, 
                 &src_width, &src_height, &offset_x, &offset_y, 
                 &scale_x, &scale_y, &interp_type);

    if (!NIL_P(interp_type))
        type = RVAL2GENUM(interp_type, GDK_TYPE_INTERP_TYPE);

    gdk_pixbuf_scale(_SELF(src), _SELF(self), 
                     NUM2INT(src_x), NUM2INT(src_y), 
                     NUM2INT(src_width), NUM2INT(src_height),
                     NUM2DBL(offset_x), NUM2DBL(offset_y),
                     NUM2DBL(scale_x), NUM2DBL(scale_y), type);
    return self;
}

static VALUE
composite_simple(VALUE self, VALUE dest_width, VALUE dest_height, VALUE interp_type, VALUE overall_alpha, VALUE check_size, VALUE color1, VALUE color2)
{
    GdkPixbuf* dest;
    VALUE ret;
    GdkInterpType type = GDK_INTERP_BILINEAR;

    if (!NIL_P(interp_type))
        type = RVAL2GENUM(interp_type, GDK_TYPE_INTERP_TYPE);

    dest = gdk_pixbuf_composite_color_simple(
        _SELF(self), NUM2INT(dest_width), NUM2INT(dest_height), 
        type, NUM2INT(overall_alpha), NUM2INT(check_size),
        NUM2UINT(color1), NUM2UINT(color2));

    if (dest == NULL)
        return Qnil;

    ret = GOBJ2RVAL(dest);
    g_object_unref(dest);
    return ret;
}

static VALUE
composite(int argc, VALUE *argv, VALUE self)
{
    VALUE ret;
    VALUE args[16];
    GdkInterpType interp_type = GDK_INTERP_BILINEAR;

    rb_scan_args(argc, argv, "97", 
                 &args[0], &args[1], &args[2], &args[3], &args[4], 
                 &args[5], &args[6], &args[7], &args[8], &args[9],
                 &args[10], &args[11], &args[12], &args[13], &args[14],
                 &args[15]);

    switch (argc) {
      case 11:
	if (!NIL_P(args[9]))
	    interp_type = RVAL2GENUM(args[9], GDK_TYPE_INTERP_TYPE);

        gdk_pixbuf_composite(_SELF(args[0]), _SELF(self), 
                             NUM2INT(args[1]), NUM2INT(args[2]),
                             NUM2INT(args[3]), NUM2INT(args[4]),
                             NUM2DBL(args[5]), NUM2DBL(args[6]),
                             NUM2DBL(args[7]), NUM2DBL(args[8]),
                             interp_type, NUM2INT(args[10]));
        ret = self;
        break;
      case 16:
	if (!NIL_P(args[9]))
	    interp_type = RVAL2GENUM(args[9], GDK_TYPE_INTERP_TYPE);

        gdk_pixbuf_composite_color(_SELF(args[0]), _SELF(self),
                                   NUM2INT(args[1]), NUM2INT(args[2]),
                                   NUM2INT(args[3]), NUM2INT(args[4]),
                                   NUM2DBL(args[5]), NUM2DBL(args[6]),
                                   NUM2DBL(args[7]), NUM2DBL(args[8]),
                                   interp_type, NUM2INT(args[10]),
                                   NUM2INT(args[11]), NUM2INT(args[12]),
                                   NUM2INT(args[13]), NUM2UINT(args[14]),
                                   NUM2UINT(args[15]));
        ret = self;
        break;
      default:
	rb_raise(rb_eArgError, "Wrong number of arguments: %d", argc);
	break;
    }
    return ret;
}

#if RBGDK_PIXBUF_CHECK_VERSION(2,6,0)
static VALUE
rotate_simple(VALUE self, VALUE angle)
{
    VALUE ret;
    GdkPixbuf* dest = gdk_pixbuf_rotate_simple(_SELF(self), RVAL2GENUM(angle, GDK_TYPE_PIXBUF_ROTATION));
    if (dest == NULL)
        return Qnil;
    ret = GOBJ2RVAL(dest);
    g_object_unref(dest);
    return ret;
}

static VALUE
flip(VALUE self, VALUE horizontal)
{
    VALUE ret;
    GdkPixbuf* dest = gdk_pixbuf_flip(_SELF(self), RVAL2CBOOL(horizontal));
    if (dest == NULL)
        return Qnil;
    ret = GOBJ2RVAL(dest);
    g_object_unref(dest);
    return ret;
}
#endif

static VALUE
add_alpha(VALUE self, VALUE substitute_color, VALUE r, VALUE g, VALUE b)
{
    VALUE ret;
    GdkPixbuf* dest = gdk_pixbuf_add_alpha(_SELF(self),
                                           RVAL2CBOOL(substitute_color),
                                           FIX2INT(r), FIX2INT(g), FIX2INT(b));
    if (dest == NULL)
        return Qnil;
    ret = GOBJ2RVAL(dest);
    g_object_unref(dest);
    return ret;
}

static VALUE
copy_area(VALUE self, VALUE src_x, VALUE src_y, VALUE width, VALUE height, VALUE dest, VALUE dest_x, VALUE dest_y)
{
    gdk_pixbuf_copy_area(_SELF(self), NUM2INT(src_x), NUM2INT(src_y),
                         NUM2INT(width), NUM2INT(height),
                         _SELF(dest), NUM2INT(dest_x), NUM2INT(dest_y));
    return dest;
}

static VALUE
saturate_and_pixelate(VALUE self, VALUE staturation, VALUE pixelate)
{
    GdkPixbuf* dest = gdk_pixbuf_copy(_SELF(self));
    gdk_pixbuf_saturate_and_pixelate(_SELF(self), dest, 
                                     NUM2DBL(staturation), RVAL2CBOOL(pixelate));
    return GOBJ2RVAL(dest);
}

static VALUE
fill(VALUE self, VALUE pixel)
{
    gdk_pixbuf_fill(_SELF(self), NUM2UINT(pixel));
    return self;
}

/* From Module Interface */
#if RBGDK_PIXBUF_CHECK_VERSION(2,2,0)
static VALUE
get_formats(G_GNUC_UNUSED VALUE self)
{
    return GSLIST2ARY2(gdk_pixbuf_get_formats(), GDK_TYPE_PIXBUF_FORMAT);
}

#ifdef HAVE_GDK_PIXBUF_SET_OPTION
static VALUE
set_option(VALUE self, VALUE key, VALUE value)
{
    return CBOOL2RVAL(gdk_pixbuf_set_option(_SELF(self),
                                            RVAL2CSTR(key), RVAL2CSTR(value)));
}
#else
static VALUE
set_option(G_GNUC_UNUSED VALUE self, G_GNUC_UNUSED VALUE key, G_GNUC_UNUSED VALUE value)
{
    rb_warning("not supported in this version of GTK+");
    return Qfalse;
}
#endif
#endif

extern void Init_gdk_pixbuf2(void);

void
Init_gdk_pixbuf2(void)
{
    VALUE mGdk = rb_define_module("Gdk");
    VALUE RG_TARGET_NAMESPACE = G_DEF_CLASS(GDK_TYPE_PIXBUF, "Pixbuf", mGdk);    

    id_pixdata = rb_intern("pixdata");
   
    /*
    gdk_rgb_init();*/ /* initialize it anyway */
    

    /*
     * Initialization and Versions 
     */
/* Removed. This crashes Ruby/GTK on Windows + GTK+-2.4.x.
   Pointed out by Laurent.
#ifdef HAVE_GDK_PIXBUF_VERSION
    rb_define_const(RG_TARGET_NAMESPACE, "VERSION", CSTR2RVAL(gdk_pixbuf_version));
#endif
*/
    rb_define_const(RG_TARGET_NAMESPACE, "MAJOR", INT2FIX(GDK_PIXBUF_MAJOR));
    rb_define_const(RG_TARGET_NAMESPACE, "MINOR", INT2FIX(GDK_PIXBUF_MINOR));
    rb_define_const(RG_TARGET_NAMESPACE, "MICRO", INT2FIX(GDK_PIXBUF_MICRO));

    /* 
     * The GdkPixbuf Structure 
     */
    G_REPLACE_GET_PROPERTY(RG_TARGET_NAMESPACE, "pixels", get_pixels, 0);
    rb_define_method(RG_TARGET_NAMESPACE, "pixels=", set_pixels, 1);
    rb_define_method(RG_TARGET_NAMESPACE, "get_option", get_option, 1);

    /* GdkPixbufError */
    G_DEF_ERROR(GDK_PIXBUF_ERROR, "PixbufError", mGdk, rb_eRuntimeError, GDK_TYPE_PIXBUF_ERROR);

    /* GdkColorspace */
    G_DEF_CLASS(GDK_TYPE_COLORSPACE, "ColorSpace", RG_TARGET_NAMESPACE);
    G_DEF_CONSTANTS(RG_TARGET_NAMESPACE, GDK_TYPE_COLORSPACE, "GDK_");

    /* GdkPixbufAlphaMode */
    G_DEF_CLASS(GDK_TYPE_PIXBUF_ALPHA_MODE, "AlphaMode", RG_TARGET_NAMESPACE);
    G_DEF_CONSTANTS(RG_TARGET_NAMESPACE, GDK_TYPE_PIXBUF_ALPHA_MODE, "GDK_PIXBUF_");

    /* 
     * File Loading, Image Data in Memory
     */
    rb_define_method(RG_TARGET_NAMESPACE, "initialize", initialize, -1);
    rb_define_method(RG_TARGET_NAMESPACE, "dup", copy, 0);
#if RBGDK_PIXBUF_CHECK_VERSION(2,4,0)
    rb_define_singleton_method(RG_TARGET_NAMESPACE, "get_file_info", get_file_info, 1);
#endif

    /*
     * File saving
     */
    rb_define_method(RG_TARGET_NAMESPACE, "save", save, -1);
#if RBGDK_PIXBUF_CHECK_VERSION(2,4,0)
    rb_define_method(RG_TARGET_NAMESPACE, "save_to_buffer", save_to_buffer, -1);
#endif

    /*
     * Scaling
     */
    rb_define_method(RG_TARGET_NAMESPACE, "scale", scale_simple, -1);
    rb_define_method(RG_TARGET_NAMESPACE, "scale!", scale, -1);
    rb_define_method(RG_TARGET_NAMESPACE, "composite", composite_simple, 7);
    rb_define_method(RG_TARGET_NAMESPACE, "composite!", composite, -1);
#if RBGDK_PIXBUF_CHECK_VERSION(2,6,0)
    rb_define_method(RG_TARGET_NAMESPACE, "rotate", rotate_simple, 1);
    rb_define_method(RG_TARGET_NAMESPACE, "flip", flip, 1);
#endif

    /* GdkInterpType */
    G_DEF_CLASS(GDK_TYPE_INTERP_TYPE, "InterpType", RG_TARGET_NAMESPACE);
    G_DEF_CONSTANTS(RG_TARGET_NAMESPACE, GDK_TYPE_INTERP_TYPE, "GDK_");

#if RBGDK_PIXBUF_CHECK_VERSION(2,6,0)
    /* GdkPixbufRotation */
    G_DEF_CLASS(GDK_TYPE_PIXBUF_ROTATION, "GdkPixbufRotation", RG_TARGET_NAMESPACE);
    G_DEF_CONSTANTS(RG_TARGET_NAMESPACE, GDK_TYPE_PIXBUF_ROTATION, "GDK_PIXBUF_");
#endif
    /*
     * Utilities
     */
    rb_define_method(RG_TARGET_NAMESPACE, "add_alpha", add_alpha, 4);
    rb_define_method(RG_TARGET_NAMESPACE, "copy_area", copy_area, 7);
    rb_define_method(RG_TARGET_NAMESPACE, "saturate_and_pixelate", saturate_and_pixelate, 2);
    rb_define_method(RG_TARGET_NAMESPACE, "fill!", fill, 1);

    /*
     * Module Interface
     */
#if RBGDK_PIXBUF_CHECK_VERSION(2,2,0)
    rb_define_singleton_method(RG_TARGET_NAMESPACE, "formats", get_formats, 0);
    rb_define_method(RG_TARGET_NAMESPACE, "set_option", set_option, 2);
#endif

    Init_gdk_pixbuf_animation(mGdk);
    Init_gdk_pixbuf_animation_iter(mGdk);
#if RBGDK_PIXBUF_CHECK_VERSION(2,8,0)
    Init_gdk_pixbuf_simpleanim(mGdk);
#endif
    Init_gdk_pixdata(mGdk);
    Init_gdk_pixbuf_loader(mGdk);
    Init_gdk_pixbuf_format(mGdk);
}
