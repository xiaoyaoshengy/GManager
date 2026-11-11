#ifndef ELAWORKSPACE_ELAWIDGETTOOLS_ELAAPPBAR_H_
#define ELAWORKSPACE_ELAWIDGETTOOLS_ELAAPPBAR_H_

#include <QWidget>

#include "ElaWidgetToolsDef.h"

#ifdef Q_OS_WIN
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
#define Q_TAKEOVER_NATIVEEVENT_H virtual bool nativeEvent(const QByteArray& eventType, void* message, qintptr* result) override;
#else
#define Q_TAKEOVER_NATIVEEVENT_H virtual bool nativeEvent(const QByteArray& eventType, void* message, long* result) override;
#endif
#else
#define Q_TAKEOVER_NATIVEEVENT_H
#endif

#ifdef Q_OS_WIN
#define ELAAPPBAR_HANDLE(ElaAppBar)                                           \
    if (ElaAppBar)                                                            \
    {                                                                         \
        int ret = ElaAppBar->takeOverNativeEvent(eventType, message, result); \
        if (ret == -1)                                                        \
        {                                                                     \
            return QWidget::nativeEvent(eventType, message, result);          \
        }                                                                     \
        return (bool)ret;                                                     \
    }                                                                         \
    return QWidget::nativeEvent(eventType, message, result);
#endif

#ifdef Q_OS_WIN
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
#define Q_TAKEOVER_NATIVEEVENT_CPP(CLASS, ElaAppBar)                                     \
    bool CLASS::nativeEvent(const QByteArray& eventType, void* message, qintptr* result) \
    {                                                                                    \
        ELAAPPBAR_HANDLE(ElaAppBar)                                                      \
    }
#else
#define Q_TAKEOVER_NATIVEEVENT_CPP(CLASS, ElaAppBar)                                  \
    bool CLASS::nativeEvent(const QByteArray& eventType, void* message, long* result) \
    {                                                                                 \
        ELAAPPBAR_HANDLE(ElaAppBar)                                                   \
    }
#endif
#else
#define Q_TAKEOVER_NATIVEEVENT_CPP(CLASS, ElaAppBar)
#endif

class QMenu;
class QShowEvent;
class ElaAppBarPrivate;
class ELA_EXPORT ElaAppBar : public QWidget
{
    Q_OBJECT
    Q_Q_CREATE(ElaAppBar)
    Q_PROPERTY_CREATE_Q_H(bool, IsStayTop)
    Q_PROPERTY_CREATE_Q_H(bool, IsFixedSize)
    Q_PROPERTY_CREATE_Q_H(bool, IsDefaultClosed)
    Q_PROPERTY_CREATE_Q_H(bool, IsOnlyAllowMinAndClose)
    Q_PROPERTY_CREATE_Q_H(int, AppBarHeight)
    Q_PROPERTY_CREATE_Q_H(int, RibbonHeight)
public:
    explicit ElaAppBar(QWidget* parent = nullptr);
    ~ElaAppBar() override;

    void setCustomWidget(ElaAppBarType::CustomArea customArea, QWidget* customWidget, QObject* hitTestObject = nullptr, const QString& hitTestFunctionName = "");
    QWidget* getCustomWidget(ElaAppBarType::CustomArea customArea) const;

    void setCustomMenu(QMenu* customMenu);
    QMenu* getCustomMenu() const;

    void setWindowButtonFlag(ElaAppBarType::ButtonType buttonFlag, bool isEnable = true);
    void setWindowButtonFlags(ElaAppBarType::ButtonFlags buttonFlags);
    ElaAppBarType::ButtonFlags getWindowButtonFlags() const;

    void setRouteBackButtonEnable(bool isEnable);
    void setRouteForwardButtonEnable(bool isEnable);

#ifdef Q_OS_WIN
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    int takeOverNativeEvent(const QByteArray& eventType, void* message, qintptr* result);
#else
    int takeOverNativeEvent(const QByteArray& eventType, void* message, long* result);
#endif
#endif
Q_SIGNALS:
    Q_SIGNAL void routeBackButtonClicked();
    Q_SIGNAL void routeForwardButtonClicked();
    Q_SIGNAL void navigationButtonClicked();
    Q_SIGNAL void themeChangeButtonClicked();
    Q_SIGNAL void closeButtonClicked();
    Q_SIGNAL void customWidgetChanged();
    Q_SIGNAL void customMenuChanged();

protected:
    bool eventFilter(QObject* obj, QEvent* event) override;
#ifndef Q_OS_WIN
    // 首次显示后检测合成器是否强制提供了服务端标题栏（如 KWin 下的 xdg-decoration）
    void showEvent(QShowEvent* event) override;
#endif
#ifdef Q_OS_WIN
    void paintEvent(QPaintEvent* event) override;
#endif

#ifndef Q_OS_WIN
private:
    // 一次性检测窗口是否存在服务端装饰，检测到后按需隐藏自绘AppBar（默认不改变外观）
    void detectServerSideDecoration();
    // 隐藏自绘AppBar并收回窗口顶部为其预留的留白（幂等，可重复调用）
    void hideAppBarForDecoration();
#endif
};

#endif // ELAWORKSPACE_ELAWIDGETTOOLS_ELAAPPBAR_H_
