#pragma once

#include <QCoreApplication>
#include <QObject>
#include <QString>
#include <QVariant>

namespace Lane
{

// Context-object shim that supplies the i18n* functions Kirigami and
// kirigami-addons QML expect on their engine root. It fills the role
// KI18n's KLocalizedContext used to: Kirigami resolves `i18n(...)` and
// friends as methods of the context object, so without one installed,
// delegates like FormTextFieldDelegate throw ReferenceErrors.
//
// Signatures mirror KLocalizedContext exactly (up to ten trailing
// QVariant substitution params) so every vendored call site binds.
// Substitutions use KI18n semantics: params fill %1..%10 in order.
//
// These are pass-throughs, not a translation framework: Lane ships one
// locale. Context variants run through QCoreApplication::translate so a
// future Linguist setup has somewhere to hook in; domain variants
// (i18nd*) ignore the domain because the vendored catalogs
// ("kirigami6", "kirigami-addons6") are not installed to load anyway.
// xi18n* markup variants substitute without markup processing, which
// is safe for plain-text results.
class I18nContext : public QObject
{
    Q_OBJECT

public:
    explicit I18nContext(QObject *parent = nullptr)
        : QObject(parent)
    {
    }

    Q_INVOKABLE QString i18n(const QString &message,
                             const QVariant &param1 = QVariant(),
                             const QVariant &param2 = QVariant(),
                             const QVariant &param3 = QVariant(),
                             const QVariant &param4 = QVariant(),
                             const QVariant &param5 = QVariant(),
                             const QVariant &param6 = QVariant(),
                             const QVariant &param7 = QVariant(),
                             const QVariant &param8 = QVariant(),
                             const QVariant &param9 = QVariant(),
                             const QVariant &param10 = QVariant()) const
    {
        return substitute(tr(message.toUtf8().constData()), {param1, param2, param3, param4, param5, param6, param7, param8, param9, param10});
    }

    Q_INVOKABLE QString i18nc(const QString &context,
                              const QString &message,
                              const QVariant &param1 = QVariant(),
                              const QVariant &param2 = QVariant(),
                              const QVariant &param3 = QVariant(),
                              const QVariant &param4 = QVariant(),
                              const QVariant &param5 = QVariant(),
                              const QVariant &param6 = QVariant(),
                              const QVariant &param7 = QVariant(),
                              const QVariant &param8 = QVariant(),
                              const QVariant &param9 = QVariant(),
                              const QVariant &param10 = QVariant()) const
    {
        return substitute(QCoreApplication::translate(context.toUtf8().constData(), message.toUtf8().constData()),
                          {param1, param2, param3, param4, param5, param6, param7, param8, param9, param10});
    }

    Q_INVOKABLE QString i18np(const QString &singular,
                              const QString &plural,
                              const QVariant &param1 = QVariant(),
                              const QVariant &param2 = QVariant(),
                              const QVariant &param3 = QVariant(),
                              const QVariant &param4 = QVariant(),
                              const QVariant &param5 = QVariant(),
                              const QVariant &param6 = QVariant(),
                              const QVariant &param7 = QVariant(),
                              const QVariant &param8 = QVariant(),
                              const QVariant &param9 = QVariant(),
                              const QVariant &param10 = QVariant()) const
    {
        const qlonglong n = param1.isValid() ? param1.toLongLong() : 0;
        return substitute(tr(singular.toUtf8().constData(), plural.toUtf8().constData(), n),
                          {param1, param2, param3, param4, param5, param6, param7, param8, param9, param10});
    }

    Q_INVOKABLE QString i18ncp(const QString &context,
                               const QString &singular,
                               const QString &plural,
                               const QVariant &param1 = QVariant(),
                               const QVariant &param2 = QVariant(),
                               const QVariant &param3 = QVariant(),
                               const QVariant &param4 = QVariant(),
                               const QVariant &param5 = QVariant(),
                               const QVariant &param6 = QVariant(),
                               const QVariant &param7 = QVariant(),
                               const QVariant &param8 = QVariant(),
                               const QVariant &param9 = QVariant(),
                               const QVariant &param10 = QVariant()) const
    {
        const qlonglong n = param1.isValid() ? param1.toLongLong() : 0;
        return substitute(QCoreApplication::translate(context.toUtf8().constData(),
                                                      singular.toUtf8().constData(),
                                                      plural.toUtf8().constData(),
                                                      n),
                          {param1, param2, param3, param4, param5, param6, param7, param8, param9, param10});
    }

    Q_INVOKABLE QString i18nd(const QString &domain,
                              const QString &message,
                              const QVariant &param1 = QVariant(),
                              const QVariant &param2 = QVariant(),
                              const QVariant &param3 = QVariant(),
                              const QVariant &param4 = QVariant(),
                              const QVariant &param5 = QVariant(),
                              const QVariant &param6 = QVariant(),
                              const QVariant &param7 = QVariant(),
                              const QVariant &param8 = QVariant(),
                              const QVariant &param9 = QVariant(),
                              const QVariant &param10 = QVariant()) const
    {
        Q_UNUSED(domain)
        return substitute(message, {param1, param2, param3, param4, param5, param6, param7, param8, param9, param10});
    }

    Q_INVOKABLE QString i18ndc(const QString &domain,
                               const QString &context,
                               const QString &message,
                               const QVariant &param1 = QVariant(),
                               const QVariant &param2 = QVariant(),
                               const QVariant &param3 = QVariant(),
                               const QVariant &param4 = QVariant(),
                               const QVariant &param5 = QVariant(),
                               const QVariant &param6 = QVariant(),
                               const QVariant &param7 = QVariant(),
                               const QVariant &param8 = QVariant(),
                               const QVariant &param9 = QVariant(),
                               const QVariant &param10 = QVariant()) const
    {
        Q_UNUSED(domain)
        return substitute(QCoreApplication::translate(context.toUtf8().constData(), message.toUtf8().constData()),
                          {param1, param2, param3, param4, param5, param6, param7, param8, param9, param10});
    }

    Q_INVOKABLE QString i18ndp(const QString &domain,
                               const QString &singular,
                               const QString &plural,
                               const QVariant &param1 = QVariant(),
                               const QVariant &param2 = QVariant(),
                               const QVariant &param3 = QVariant(),
                               const QVariant &param4 = QVariant(),
                               const QVariant &param5 = QVariant(),
                               const QVariant &param6 = QVariant(),
                               const QVariant &param7 = QVariant(),
                               const QVariant &param8 = QVariant(),
                               const QVariant &param9 = QVariant(),
                               const QVariant &param10 = QVariant()) const
    {
        Q_UNUSED(domain)
        const qlonglong n = param1.isValid() ? param1.toLongLong() : 0;
        return substitute(n == 1 ? singular : plural, {param1, param2, param3, param4, param5, param6, param7, param8, param9, param10});
    }

    Q_INVOKABLE QString i18ndcp(const QString &domain,
                                const QString &context,
                                const QString &singular,
                                const QString &plural,
                                const QVariant &param1 = QVariant(),
                                const QVariant &param2 = QVariant(),
                                const QVariant &param3 = QVariant(),
                                const QVariant &param4 = QVariant(),
                                const QVariant &param5 = QVariant(),
                                const QVariant &param6 = QVariant(),
                                const QVariant &param7 = QVariant(),
                                const QVariant &param8 = QVariant(),
                                const QVariant &param9 = QVariant(),
                                const QVariant &param10 = QVariant()) const
    {
        Q_UNUSED(domain)
        const qlonglong n = param1.isValid() ? param1.toLongLong() : 0;
        return substitute(QCoreApplication::translate(context.toUtf8().constData(),
                                                      n == 1 ? singular.toUtf8().constData() : plural.toUtf8().constData()),
                          {param1, param2, param3, param4, param5, param6, param7, param8, param9, param10});
    }

    // xi18n* markup variants: same substitution, no markup processing.
    Q_INVOKABLE QString xi18n(const QString &message,
                              const QVariant &param1 = QVariant(),
                              const QVariant &param2 = QVariant(),
                              const QVariant &param3 = QVariant(),
                              const QVariant &param4 = QVariant(),
                              const QVariant &param5 = QVariant(),
                              const QVariant &param6 = QVariant(),
                              const QVariant &param7 = QVariant(),
                              const QVariant &param8 = QVariant(),
                              const QVariant &param9 = QVariant(),
                              const QVariant &param10 = QVariant()) const
    {
        return i18n(message, param1, param2, param3, param4, param5, param6, param7, param8, param9, param10);
    }

    Q_INVOKABLE QString xi18nc(const QString &context,
                               const QString &message,
                               const QVariant &param1 = QVariant(),
                               const QVariant &param2 = QVariant(),
                               const QVariant &param3 = QVariant(),
                               const QVariant &param4 = QVariant(),
                               const QVariant &param5 = QVariant(),
                               const QVariant &param6 = QVariant(),
                               const QVariant &param7 = QVariant(),
                               const QVariant &param8 = QVariant(),
                               const QVariant &param9 = QVariant(),
                               const QVariant &param10 = QVariant()) const
    {
        return i18nc(context, message, param1, param2, param3, param4, param5, param6, param7, param8, param9, param10);
    }

    Q_INVOKABLE QString xi18np(const QString &singular,
                               const QString &plural,
                               const QVariant &param1 = QVariant(),
                               const QVariant &param2 = QVariant(),
                               const QVariant &param3 = QVariant(),
                               const QVariant &param4 = QVariant(),
                               const QVariant &param5 = QVariant(),
                               const QVariant &param6 = QVariant(),
                               const QVariant &param7 = QVariant(),
                               const QVariant &param8 = QVariant(),
                               const QVariant &param9 = QVariant(),
                               const QVariant &param10 = QVariant()) const
    {
        return i18np(singular, plural, param1, param2, param3, param4, param5, param6, param7, param8, param9, param10);
    }

    Q_INVOKABLE QString xi18ncp(const QString &context,
                                const QString &singular,
                                const QString &plural,
                                const QVariant &param1 = QVariant(),
                                const QVariant &param2 = QVariant(),
                                const QVariant &param3 = QVariant(),
                                const QVariant &param4 = QVariant(),
                                const QVariant &param5 = QVariant(),
                                const QVariant &param6 = QVariant(),
                                const QVariant &param7 = QVariant(),
                                const QVariant &param8 = QVariant(),
                                const QVariant &param9 = QVariant(),
                                const QVariant &param10 = QVariant()) const
    {
        return i18ncp(context, singular, plural, param1, param2, param3, param4, param5, param6, param7, param8, param9, param10);
    }

    Q_INVOKABLE QString xi18nd(const QString &domain,
                               const QString &message,
                               const QVariant &param1 = QVariant(),
                               const QVariant &param2 = QVariant(),
                               const QVariant &param3 = QVariant(),
                               const QVariant &param4 = QVariant(),
                               const QVariant &param5 = QVariant(),
                               const QVariant &param6 = QVariant(),
                               const QVariant &param7 = QVariant(),
                               const QVariant &param8 = QVariant(),
                               const QVariant &param9 = QVariant(),
                               const QVariant &param10 = QVariant()) const
    {
        return i18nd(domain, message, param1, param2, param3, param4, param5, param6, param7, param8, param9, param10);
    }

    Q_INVOKABLE QString xi18ndc(const QString &domain,
                                const QString &context,
                                const QString &message,
                                const QVariant &param1 = QVariant(),
                                const QVariant &param2 = QVariant(),
                                const QVariant &param3 = QVariant(),
                                const QVariant &param4 = QVariant(),
                                const QVariant &param5 = QVariant(),
                                const QVariant &param6 = QVariant(),
                                const QVariant &param7 = QVariant(),
                                const QVariant &param8 = QVariant(),
                                const QVariant &param9 = QVariant(),
                                const QVariant &param10 = QVariant()) const
    {
        return i18ndc(domain, context, message, param1, param2, param3, param4, param5, param6, param7, param8, param9, param10);
    }

    Q_INVOKABLE QString xi18ndp(const QString &domain,
                                const QString &singular,
                                const QString &plural,
                                const QVariant &param1 = QVariant(),
                                const QVariant &param2 = QVariant(),
                                const QVariant &param3 = QVariant(),
                                const QVariant &param4 = QVariant(),
                                const QVariant &param5 = QVariant(),
                                const QVariant &param6 = QVariant(),
                                const QVariant &param7 = QVariant(),
                                const QVariant &param8 = QVariant(),
                                const QVariant &param9 = QVariant(),
                                const QVariant &param10 = QVariant()) const
    {
        return i18ndp(domain, singular, plural, param1, param2, param3, param4, param5, param6, param7, param8, param9, param10);
    }

    Q_INVOKABLE QString xi18ndcp(const QString &domain,
                                 const QString &context,
                                 const QString &singular,
                                 const QString &plural,
                                 const QVariant &param1 = QVariant(),
                                 const QVariant &param2 = QVariant(),
                                 const QVariant &param3 = QVariant(),
                                 const QVariant &param4 = QVariant(),
                                 const QVariant &param5 = QVariant(),
                                 const QVariant &param6 = QVariant(),
                                 const QVariant &param7 = QVariant(),
                                 const QVariant &param8 = QVariant(),
                                 const QVariant &param9 = QVariant(),
                                 const QVariant &param10 = QVariant()) const
    {
        return i18ndcp(domain, context, singular, plural, param1, param2, param3, param4, param5, param6, param7, param8, param9, param10);
    }

private:
    // KI18n semantics: each valid param fills %1..%10 in order;
    // invalid (unset) params leave their placeholders untouched.
    static QString substitute(const QString &text, const QList<QVariant> &params)
    {
        QString result = text;
        for (int i = 0; i < params.size(); ++i) {
            if (params.at(i).isValid()) {
                result.replace(QStringLiteral("%%1").arg(i + 1), params.at(i).toString());
            }
        }
        return result;
    }
};

} // namespace Lane
