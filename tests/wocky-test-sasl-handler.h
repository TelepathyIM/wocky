#ifndef _WOCKY_TEST_SASL_HANDLER_H
#define _WOCKY_TEST_SASL_HANDLER_H

#include <glib-object.h>

G_BEGIN_DECLS

#define WOCKY_TYPE_TEST_SASL_HANDLER wocky_test_sasl_handler_get_type()

#define WOCKY_TEST_SASL_HANDLER(obj) \
    (G_TYPE_CHECK_INSTANCE_CAST ((obj), WOCKY_TYPE_TEST_SASL_HANDLER, \
        WockyTestSaslHandler))

#define WOCKY_TEST_SASL_HANDLER_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_CAST ((klass), WOCKY_TYPE_TEST_SASL_HANDLER, \
        WockyTestSaslHandlerClass))

#define WOCKY_IS_TEST_SASL_HANDLER(obj) \
    (G_TYPE_CHECK_INSTANCE_TYPE ((obj), WOCKY_TYPE_TEST_SASL_HANDLER))

#define WOCKY_IS_TEST_SASL_HANDLER_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_TYPE ((klass), WOCKY_TYPE_TEST_SASL_HANDLER))

#define WOCKY_TEST_SASL_HANDLER_GET_CLASS(obj) \
    (G_TYPE_INSTANCE_GET_CLASS ((obj), WOCKY_TYPE_TEST_SASL_HANDLER, \
        WockyTestSaslHandlerClass))

typedef struct {
  GObject parent;
} WockyTestSaslHandler;

typedef struct {
  GObjectClass parent_class;
} WockyTestSaslHandlerClass;

GType wocky_test_sasl_handler_get_type (void);

WockyTestSaslHandler* wocky_test_sasl_handler_new (void);

G_END_DECLS

#endif /* _WOCKY_TEST_SASL_HANDLER_H */
