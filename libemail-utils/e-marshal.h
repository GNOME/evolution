
#ifndef __e_marshal_MARSHAL_H__
#define __e_marshal_MARSHAL_H__

#include	<glib-object.h>

G_BEGIN_DECLS

/* BOOLEAN:BOXED (e-marshal.list:1) */
extern void e_marshal_BOOLEAN__BOXED (GClosure     *closure,
                                      GValue       *return_value,
                                      guint         n_param_values,
                                      const GValue *param_values,
                                      gpointer      invocation_hint,
                                      gpointer      marshal_data);

/* BOOLEAN:BOXED,STRING (e-marshal.list:2) */
extern void e_marshal_BOOLEAN__BOXED_STRING (GClosure     *closure,
                                             GValue       *return_value,
                                             guint         n_param_values,
                                             const GValue *param_values,
                                             gpointer      invocation_hint,
                                             gpointer      marshal_data);

/* BOOLEAN:BOXED,POINTER,POINTER (e-marshal.list:3) */
extern void e_marshal_BOOLEAN__BOXED_POINTER_POINTER (GClosure     *closure,
                                                      GValue       *return_value,
                                                      guint         n_param_values,
                                                      const GValue *param_values,
                                                      gpointer      invocation_hint,
                                                      gpointer      marshal_data);

/* BOOLEAN:INT,INT,BOXED (e-marshal.list:4) */
extern void e_marshal_BOOLEAN__INT_INT_BOXED (GClosure     *closure,
                                              GValue       *return_value,
                                              guint         n_param_values,
                                              const GValue *param_values,
                                              gpointer      invocation_hint,
                                              gpointer      marshal_data);

/* BOOLEAN:INT,INT,OBJECT,INT,INT,UINT (e-marshal.list:5) */
extern void e_marshal_BOOLEAN__INT_INT_OBJECT_INT_INT_UINT (GClosure     *closure,
                                                            GValue       *return_value,
                                                            guint         n_param_values,
                                                            const GValue *param_values,
                                                            gpointer      invocation_hint,
                                                            gpointer      marshal_data);

/* BOOLEAN:INT,POINTER,INT,BOXED (e-marshal.list:6) */
extern void e_marshal_BOOLEAN__INT_POINTER_INT_BOXED (GClosure     *closure,
                                                      GValue       *return_value,
                                                      guint         n_param_values,
                                                      const GValue *param_values,
                                                      gpointer      invocation_hint,
                                                      gpointer      marshal_data);

/* BOOLEAN:INT,POINTER,INT,OBJECT,INT,INT,UINT (e-marshal.list:7) */
extern void e_marshal_BOOLEAN__INT_POINTER_INT_OBJECT_INT_INT_UINT (GClosure     *closure,
                                                                    GValue       *return_value,
                                                                    guint         n_param_values,
                                                                    const GValue *param_values,
                                                                    gpointer      invocation_hint,
                                                                    gpointer      marshal_data);

/* BOOLEAN:NONE (e-marshal.list:8) */
extern void e_marshal_BOOLEAN__VOID (GClosure     *closure,
                                     GValue       *return_value,
                                     guint         n_param_values,
                                     const GValue *param_values,
                                     gpointer      invocation_hint,
                                     gpointer      marshal_data);
#define e_marshal_BOOLEAN__NONE	e_marshal_BOOLEAN__VOID

/* BOOLEAN:OBJECT (e-marshal.list:9) */
extern void e_marshal_BOOLEAN__OBJECT (GClosure     *closure,
                                       GValue       *return_value,
                                       guint         n_param_values,
                                       const GValue *param_values,
                                       gpointer      invocation_hint,
                                       gpointer      marshal_data);

/* BOOLEAN:OBJECT,DOUBLE,DOUBLE,BOOLEAN (e-marshal.list:10) */
extern void e_marshal_BOOLEAN__OBJECT_DOUBLE_DOUBLE_BOOLEAN (GClosure     *closure,
                                                             GValue       *return_value,
                                                             guint         n_param_values,
                                                             const GValue *param_values,
                                                             gpointer      invocation_hint,
                                                             gpointer      marshal_data);

/* BOOLEAN:POINTER (e-marshal.list:11) */
extern void e_marshal_BOOLEAN__POINTER (GClosure     *closure,
                                        GValue       *return_value,
                                        guint         n_param_values,
                                        const GValue *param_values,
                                        gpointer      invocation_hint,
                                        gpointer      marshal_data);

/* BOOLEAN:POINTER,BOOLEAN,POINTER (e-marshal.list:12) */
extern void e_marshal_BOOLEAN__POINTER_BOOLEAN_POINTER (GClosure     *closure,
                                                        GValue       *return_value,
                                                        guint         n_param_values,
                                                        const GValue *param_values,
                                                        gpointer      invocation_hint,
                                                        gpointer      marshal_data);

/* BOOLEAN:POINTER,POINTER (e-marshal.list:13) */
extern void e_marshal_BOOLEAN__POINTER_POINTER (GClosure     *closure,
                                                GValue       *return_value,
                                                guint         n_param_values,
                                                const GValue *param_values,
                                                gpointer      invocation_hint,
                                                gpointer      marshal_data);

/* BOOLEAN:POINTER,POINTER,POINTER,POINTER (e-marshal.list:14) */
extern void e_marshal_BOOLEAN__POINTER_POINTER_POINTER_POINTER (GClosure     *closure,
                                                                GValue       *return_value,
                                                                guint         n_param_values,
                                                                const GValue *param_values,
                                                                gpointer      invocation_hint,
                                                                gpointer      marshal_data);

/* BOOLEAN:STRING (e-marshal.list:15) */
extern void e_marshal_BOOLEAN__STRING (GClosure     *closure,
                                       GValue       *return_value,
                                       guint         n_param_values,
                                       const GValue *param_values,
                                       gpointer      invocation_hint,
                                       gpointer      marshal_data);

/* BOOLEAN:STRING,INT (e-marshal.list:16) */
extern void e_marshal_BOOLEAN__STRING_INT (GClosure     *closure,
                                           GValue       *return_value,
                                           guint         n_param_values,
                                           const GValue *param_values,
                                           gpointer      invocation_hint,
                                           gpointer      marshal_data);

/* DOUBLE:OBJECT,DOUBLE,DOUBLE,BOOLEAN (e-marshal.list:17) */
extern void e_marshal_DOUBLE__OBJECT_DOUBLE_DOUBLE_BOOLEAN (GClosure     *closure,
                                                            GValue       *return_value,
                                                            guint         n_param_values,
                                                            const GValue *param_values,
                                                            gpointer      invocation_hint,
                                                            gpointer      marshal_data);

/* INT:BOXED (e-marshal.list:18) */
extern void e_marshal_INT__BOXED (GClosure     *closure,
                                  GValue       *return_value,
                                  guint         n_param_values,
                                  const GValue *param_values,
                                  gpointer      invocation_hint,
                                  gpointer      marshal_data);

/* INT:INT (e-marshal.list:19) */
extern void e_marshal_INT__INT (GClosure     *closure,
                                GValue       *return_value,
                                guint         n_param_values,
                                const GValue *param_values,
                                gpointer      invocation_hint,
                                gpointer      marshal_data);

/* INT:INT,INT,BOXED (e-marshal.list:20) */
extern void e_marshal_INT__INT_INT_BOXED (GClosure     *closure,
                                          GValue       *return_value,
                                          guint         n_param_values,
                                          const GValue *param_values,
                                          gpointer      invocation_hint,
                                          gpointer      marshal_data);

/* INT:INT,POINTER,INT,BOXED (e-marshal.list:21) */
extern void e_marshal_INT__INT_POINTER_INT_BOXED (GClosure     *closure,
                                                  GValue       *return_value,
                                                  guint         n_param_values,
                                                  const GValue *param_values,
                                                  gpointer      invocation_hint,
                                                  gpointer      marshal_data);

/* INT:OBJECT,BOXED (e-marshal.list:22) */
extern void e_marshal_INT__OBJECT_BOXED (GClosure     *closure,
                                         GValue       *return_value,
                                         guint         n_param_values,
                                         const GValue *param_values,
                                         gpointer      invocation_hint,
                                         gpointer      marshal_data);

/* INT:POINTER (e-marshal.list:23) */
extern void e_marshal_INT__POINTER (GClosure     *closure,
                                    GValue       *return_value,
                                    guint         n_param_values,
                                    const GValue *param_values,
                                    gpointer      invocation_hint,
                                    gpointer      marshal_data);

/* NONE:BOXED,INT (e-marshal.list:24) */
extern void e_marshal_VOID__BOXED_INT (GClosure     *closure,
                                       GValue       *return_value,
                                       guint         n_param_values,
                                       const GValue *param_values,
                                       gpointer      invocation_hint,
                                       gpointer      marshal_data);
#define e_marshal_NONE__BOXED_INT	e_marshal_VOID__BOXED_INT

/* NONE:ENUM,OBJECT,OBJECT (e-marshal.list:25) */
extern void e_marshal_VOID__ENUM_OBJECT_OBJECT (GClosure     *closure,
                                                GValue       *return_value,
                                                guint         n_param_values,
                                                const GValue *param_values,
                                                gpointer      invocation_hint,
                                                gpointer      marshal_data);
#define e_marshal_NONE__ENUM_OBJECT_OBJECT	e_marshal_VOID__ENUM_OBJECT_OBJECT

/* NONE:INT,INT (e-marshal.list:26) */
extern void e_marshal_VOID__INT_INT (GClosure     *closure,
                                     GValue       *return_value,
                                     guint         n_param_values,
                                     const GValue *param_values,
                                     gpointer      invocation_hint,
                                     gpointer      marshal_data);
#define e_marshal_NONE__INT_INT	e_marshal_VOID__INT_INT

/* NONE:INT,INT,BOXED (e-marshal.list:27) */
extern void e_marshal_VOID__INT_INT_BOXED (GClosure     *closure,
                                           GValue       *return_value,
                                           guint         n_param_values,
                                           const GValue *param_values,
                                           gpointer      invocation_hint,
                                           gpointer      marshal_data);
#define e_marshal_NONE__INT_INT_BOXED	e_marshal_VOID__INT_INT_BOXED

/* NONE:INT,INT,OBJECT (e-marshal.list:28) */
extern void e_marshal_VOID__INT_INT_OBJECT (GClosure     *closure,
                                            GValue       *return_value,
                                            guint         n_param_values,
                                            const GValue *param_values,
                                            gpointer      invocation_hint,
                                            gpointer      marshal_data);
#define e_marshal_NONE__INT_INT_OBJECT	e_marshal_VOID__INT_INT_OBJECT

/* NONE:INT,INT,OBJECT,BOXED,UINT,UINT (e-marshal.list:29) */
extern void e_marshal_VOID__INT_INT_OBJECT_BOXED_UINT_UINT (GClosure     *closure,
                                                            GValue       *return_value,
                                                            guint         n_param_values,
                                                            const GValue *param_values,
                                                            gpointer      invocation_hint,
                                                            gpointer      marshal_data);
#define e_marshal_NONE__INT_INT_OBJECT_BOXED_UINT_UINT	e_marshal_VOID__INT_INT_OBJECT_BOXED_UINT_UINT

/* NONE:INT,INT,OBJECT,INT,INT,BOXED,UINT,UINT (e-marshal.list:30) */
extern void e_marshal_VOID__INT_INT_OBJECT_INT_INT_BOXED_UINT_UINT (GClosure     *closure,
                                                                    GValue       *return_value,
                                                                    guint         n_param_values,
                                                                    const GValue *param_values,
                                                                    gpointer      invocation_hint,
                                                                    gpointer      marshal_data);
#define e_marshal_NONE__INT_INT_OBJECT_INT_INT_BOXED_UINT_UINT	e_marshal_VOID__INT_INT_OBJECT_INT_INT_BOXED_UINT_UINT

/* NONE:INT,INT,OBJECT,UINT (e-marshal.list:31) */
extern void e_marshal_VOID__INT_INT_OBJECT_UINT (GClosure     *closure,
                                                 GValue       *return_value,
                                                 guint         n_param_values,
                                                 const GValue *param_values,
                                                 gpointer      invocation_hint,
                                                 gpointer      marshal_data);
#define e_marshal_NONE__INT_INT_OBJECT_UINT	e_marshal_VOID__INT_INT_OBJECT_UINT

/* NONE:INT,OBJECT (e-marshal.list:32) */
extern void e_marshal_VOID__INT_OBJECT (GClosure     *closure,
                                        GValue       *return_value,
                                        guint         n_param_values,
                                        const GValue *param_values,
                                        gpointer      invocation_hint,
                                        gpointer      marshal_data);
#define e_marshal_NONE__INT_OBJECT	e_marshal_VOID__INT_OBJECT

/* NONE:INT,POINTER (e-marshal.list:33) */
extern void e_marshal_VOID__INT_POINTER (GClosure     *closure,
                                         GValue       *return_value,
                                         guint         n_param_values,
                                         const GValue *param_values,
                                         gpointer      invocation_hint,
                                         gpointer      marshal_data);
#define e_marshal_NONE__INT_POINTER	e_marshal_VOID__INT_POINTER

/* NONE:INT,POINTER,INT,BOXED (e-marshal.list:34) */
extern void e_marshal_VOID__INT_POINTER_INT_BOXED (GClosure     *closure,
                                                   GValue       *return_value,
                                                   guint         n_param_values,
                                                   const GValue *param_values,
                                                   gpointer      invocation_hint,
                                                   gpointer      marshal_data);
#define e_marshal_NONE__INT_POINTER_INT_BOXED	e_marshal_VOID__INT_POINTER_INT_BOXED

/* NONE:INT,POINTER,INT,OBJECT (e-marshal.list:35) */
extern void e_marshal_VOID__INT_POINTER_INT_OBJECT (GClosure     *closure,
                                                    GValue       *return_value,
                                                    guint         n_param_values,
                                                    const GValue *param_values,
                                                    gpointer      invocation_hint,
                                                    gpointer      marshal_data);
#define e_marshal_NONE__INT_POINTER_INT_OBJECT	e_marshal_VOID__INT_POINTER_INT_OBJECT

/* NONE:INT,POINTER,INT,OBJECT,BOXED,UINT,UINT (e-marshal.list:36) */
extern void e_marshal_VOID__INT_POINTER_INT_OBJECT_BOXED_UINT_UINT (GClosure     *closure,
                                                                    GValue       *return_value,
                                                                    guint         n_param_values,
                                                                    const GValue *param_values,
                                                                    gpointer      invocation_hint,
                                                                    gpointer      marshal_data);
#define e_marshal_NONE__INT_POINTER_INT_OBJECT_BOXED_UINT_UINT	e_marshal_VOID__INT_POINTER_INT_OBJECT_BOXED_UINT_UINT

/* NONE:INT,POINTER,INT,OBJECT,INT,INT,BOXED,UINT,UINT (e-marshal.list:37) */
extern void e_marshal_VOID__INT_POINTER_INT_OBJECT_INT_INT_BOXED_UINT_UINT (GClosure     *closure,
                                                                            GValue       *return_value,
                                                                            guint         n_param_values,
                                                                            const GValue *param_values,
                                                                            gpointer      invocation_hint,
                                                                            gpointer      marshal_data);
#define e_marshal_NONE__INT_POINTER_INT_OBJECT_INT_INT_BOXED_UINT_UINT	e_marshal_VOID__INT_POINTER_INT_OBJECT_INT_INT_BOXED_UINT_UINT

/* NONE:INT,POINTER,INT,OBJECT,UINT (e-marshal.list:38) */
extern void e_marshal_VOID__INT_POINTER_INT_OBJECT_UINT (GClosure     *closure,
                                                         GValue       *return_value,
                                                         guint         n_param_values,
                                                         const GValue *param_values,
                                                         gpointer      invocation_hint,
                                                         gpointer      marshal_data);
#define e_marshal_NONE__INT_POINTER_INT_OBJECT_UINT	e_marshal_VOID__INT_POINTER_INT_OBJECT_UINT

/* NONE:LONG,LONG (e-marshal.list:39) */
extern void e_marshal_VOID__LONG_LONG (GClosure     *closure,
                                       GValue       *return_value,
                                       guint         n_param_values,
                                       const GValue *param_values,
                                       gpointer      invocation_hint,
                                       gpointer      marshal_data);
#define e_marshal_NONE__LONG_LONG	e_marshal_VOID__LONG_LONG

/* NONE:OBJECT,BOOLEAN (e-marshal.list:40) */
extern void e_marshal_VOID__OBJECT_BOOLEAN (GClosure     *closure,
                                            GValue       *return_value,
                                            guint         n_param_values,
                                            const GValue *param_values,
                                            gpointer      invocation_hint,
                                            gpointer      marshal_data);
#define e_marshal_NONE__OBJECT_BOOLEAN	e_marshal_VOID__OBJECT_BOOLEAN

/* NONE:OBJECT,DOUBLE,DOUBLE,BOOLEAN (e-marshal.list:41) */
extern void e_marshal_VOID__OBJECT_DOUBLE_DOUBLE_BOOLEAN (GClosure     *closure,
                                                          GValue       *return_value,
                                                          guint         n_param_values,
                                                          const GValue *param_values,
                                                          gpointer      invocation_hint,
                                                          gpointer      marshal_data);
#define e_marshal_NONE__OBJECT_DOUBLE_DOUBLE_BOOLEAN	e_marshal_VOID__OBJECT_DOUBLE_DOUBLE_BOOLEAN

/* NONE:OBJECT,OBJECT (e-marshal.list:42) */
extern void e_marshal_VOID__OBJECT_OBJECT (GClosure     *closure,
                                           GValue       *return_value,
                                           guint         n_param_values,
                                           const GValue *param_values,
                                           gpointer      invocation_hint,
                                           gpointer      marshal_data);
#define e_marshal_NONE__OBJECT_OBJECT	e_marshal_VOID__OBJECT_OBJECT

/* NONE:OBJECT,STRING (e-marshal.list:43) */
extern void e_marshal_VOID__OBJECT_STRING (GClosure     *closure,
                                           GValue       *return_value,
                                           guint         n_param_values,
                                           const GValue *param_values,
                                           gpointer      invocation_hint,
                                           gpointer      marshal_data);
#define e_marshal_NONE__OBJECT_STRING	e_marshal_VOID__OBJECT_STRING

/* NONE:OBJECT,STRING,INT (e-marshal.list:44) */
extern void e_marshal_VOID__OBJECT_STRING_INT (GClosure     *closure,
                                               GValue       *return_value,
                                               guint         n_param_values,
                                               const GValue *param_values,
                                               gpointer      invocation_hint,
                                               gpointer      marshal_data);
#define e_marshal_NONE__OBJECT_STRING_INT	e_marshal_VOID__OBJECT_STRING_INT

/* NONE:OBJECT,STRING,INT,STRING,STRING,STRING (e-marshal.list:45) */
extern void e_marshal_VOID__OBJECT_STRING_INT_STRING_STRING_STRING (GClosure     *closure,
                                                                    GValue       *return_value,
                                                                    guint         n_param_values,
                                                                    const GValue *param_values,
                                                                    gpointer      invocation_hint,
                                                                    gpointer      marshal_data);
#define e_marshal_NONE__OBJECT_STRING_INT_STRING_STRING_STRING	e_marshal_VOID__OBJECT_STRING_INT_STRING_STRING_STRING

/* NONE:OBJECT,STRING,STRING (e-marshal.list:46) */
extern void e_marshal_VOID__OBJECT_STRING_STRING (GClosure     *closure,
                                                  GValue       *return_value,
                                                  guint         n_param_values,
                                                  const GValue *param_values,
                                                  gpointer      invocation_hint,
                                                  gpointer      marshal_data);
#define e_marshal_NONE__OBJECT_STRING_STRING	e_marshal_VOID__OBJECT_STRING_STRING

/* NONE:OBJECT,STRING,UINT (e-marshal.list:47) */
extern void e_marshal_VOID__OBJECT_STRING_UINT (GClosure     *closure,
                                                GValue       *return_value,
                                                guint         n_param_values,
                                                const GValue *param_values,
                                                gpointer      invocation_hint,
                                                gpointer      marshal_data);
#define e_marshal_NONE__OBJECT_STRING_UINT	e_marshal_VOID__OBJECT_STRING_UINT

/* NONE:POINTER,INT (e-marshal.list:48) */
extern void e_marshal_VOID__POINTER_INT (GClosure     *closure,
                                         GValue       *return_value,
                                         guint         n_param_values,
                                         const GValue *param_values,
                                         gpointer      invocation_hint,
                                         gpointer      marshal_data);
#define e_marshal_NONE__POINTER_INT	e_marshal_VOID__POINTER_INT

/* NONE:POINTER,INT,INT,INT,INT (e-marshal.list:49) */
extern void e_marshal_VOID__POINTER_INT_INT_INT_INT (GClosure     *closure,
                                                     GValue       *return_value,
                                                     guint         n_param_values,
                                                     const GValue *param_values,
                                                     gpointer      invocation_hint,
                                                     gpointer      marshal_data);
#define e_marshal_NONE__POINTER_INT_INT_INT_INT	e_marshal_VOID__POINTER_INT_INT_INT_INT

/* NONE:POINTER,INT,OBJECT (e-marshal.list:50) */
extern void e_marshal_VOID__POINTER_INT_OBJECT (GClosure     *closure,
                                                GValue       *return_value,
                                                guint         n_param_values,
                                                const GValue *param_values,
                                                gpointer      invocation_hint,
                                                gpointer      marshal_data);
#define e_marshal_NONE__POINTER_INT_OBJECT	e_marshal_VOID__POINTER_INT_OBJECT

/* NONE:POINTER,OBJECT (e-marshal.list:51) */
extern void e_marshal_VOID__POINTER_OBJECT (GClosure     *closure,
                                            GValue       *return_value,
                                            guint         n_param_values,
                                            const GValue *param_values,
                                            gpointer      invocation_hint,
                                            gpointer      marshal_data);
#define e_marshal_NONE__POINTER_OBJECT	e_marshal_VOID__POINTER_OBJECT

/* NONE:POINTER,POINTER (e-marshal.list:52) */
extern void e_marshal_VOID__POINTER_POINTER (GClosure     *closure,
                                             GValue       *return_value,
                                             guint         n_param_values,
                                             const GValue *param_values,
                                             gpointer      invocation_hint,
                                             gpointer      marshal_data);
#define e_marshal_NONE__POINTER_POINTER	e_marshal_VOID__POINTER_POINTER

/* NONE:POINTER,POINTER,INT (e-marshal.list:53) */
extern void e_marshal_VOID__POINTER_POINTER_INT (GClosure     *closure,
                                                 GValue       *return_value,
                                                 guint         n_param_values,
                                                 const GValue *param_values,
                                                 gpointer      invocation_hint,
                                                 gpointer      marshal_data);
#define e_marshal_NONE__POINTER_POINTER_INT	e_marshal_VOID__POINTER_POINTER_INT

/* NONE:STRING,DOUBLE (e-marshal.list:54) */
extern void e_marshal_VOID__STRING_DOUBLE (GClosure     *closure,
                                           GValue       *return_value,
                                           guint         n_param_values,
                                           const GValue *param_values,
                                           gpointer      invocation_hint,
                                           gpointer      marshal_data);
#define e_marshal_NONE__STRING_DOUBLE	e_marshal_VOID__STRING_DOUBLE

/* NONE:STRING,INT (e-marshal.list:55) */
extern void e_marshal_VOID__STRING_INT (GClosure     *closure,
                                        GValue       *return_value,
                                        guint         n_param_values,
                                        const GValue *param_values,
                                        gpointer      invocation_hint,
                                        gpointer      marshal_data);
#define e_marshal_NONE__STRING_INT	e_marshal_VOID__STRING_INT

/* NONE:STRING,INT,INT (e-marshal.list:56) */
extern void e_marshal_VOID__STRING_INT_INT (GClosure     *closure,
                                            GValue       *return_value,
                                            guint         n_param_values,
                                            const GValue *param_values,
                                            gpointer      invocation_hint,
                                            gpointer      marshal_data);
#define e_marshal_NONE__STRING_INT_INT	e_marshal_VOID__STRING_INT_INT

/* NONE:STRING,POINTER,POINTER (e-marshal.list:57) */
extern void e_marshal_VOID__STRING_POINTER_POINTER (GClosure     *closure,
                                                    GValue       *return_value,
                                                    guint         n_param_values,
                                                    const GValue *param_values,
                                                    gpointer      invocation_hint,
                                                    gpointer      marshal_data);
#define e_marshal_NONE__STRING_POINTER_POINTER	e_marshal_VOID__STRING_POINTER_POINTER

/* NONE:STRING,STRING (e-marshal.list:58) */
extern void e_marshal_VOID__STRING_STRING (GClosure     *closure,
                                           GValue       *return_value,
                                           guint         n_param_values,
                                           const GValue *param_values,
                                           gpointer      invocation_hint,
                                           gpointer      marshal_data);
#define e_marshal_NONE__STRING_STRING	e_marshal_VOID__STRING_STRING

/* NONE:UINT,STRING (e-marshal.list:59) */
extern void e_marshal_VOID__UINT_STRING (GClosure     *closure,
                                         GValue       *return_value,
                                         guint         n_param_values,
                                         const GValue *param_values,
                                         gpointer      invocation_hint,
                                         gpointer      marshal_data);
#define e_marshal_NONE__UINT_STRING	e_marshal_VOID__UINT_STRING

/* STRING:NONE (e-marshal.list:60) */
extern void e_marshal_STRING__VOID (GClosure     *closure,
                                    GValue       *return_value,
                                    guint         n_param_values,
                                    const GValue *param_values,
                                    gpointer      invocation_hint,
                                    gpointer      marshal_data);
#define e_marshal_STRING__NONE	e_marshal_STRING__VOID

G_END_DECLS

#endif /* __e_marshal_MARSHAL_H__ */

