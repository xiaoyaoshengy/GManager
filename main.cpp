#include <QApplication>
#include <QLocalServer>
#include <QLocalSocket>
#include <QScreen>

#include "ElaApplication.h"
// #include "ElaLog.h"

#include "mainwindow.h"

#ifdef Q_OS_WIN
#include <Windows.h>
#endif

namespace {

// 单实例通信用的本地套接字名（Linux 下是 /tmp 下的 socket 文件）
const QString kSingleInstanceKey = QStringLiteral("GManager-SingleInstance");

// 已有实例正在运行：通知它显示窗口，随后本进程直接退出
bool notifyRunningInstance() {
  QLocalSocket probe;
  probe.connectToServer(kSingleInstanceKey, QIODevice::WriteOnly);
  if (!probe.waitForConnected(300)) {
    return false;
  }
  probe.write("activate");
  probe.flush();
  probe.waitForBytesWritten(300);
  probe.disconnectFromServer();
  return true;
}

// 把已运行的窗口从隐藏/最小化状态恢复到前台
void focusMainWindow(QWidget *window) {
  if (!window) {
    return;
  }
  window->showNormal();
  window->raise();
  window->activateWindow();
}

} // namespace

int main(int argc, char *argv[]) {
#if (QT_VERSION < QT_VERSION_CHECK(6, 0, 0))
  QGuiApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);
#if (QT_VERSION >= QT_VERSION_CHECK(5, 14, 0))
  QGuiApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
  QGuiApplication::setHighDpiScaleFactorRoundingPolicy(
      Qt::HighDpiScaleFactorRoundingPolicy::PassThrough);
#else
  // 根据实际屏幕缩放比例更改
  qputenv("QT_SCALE_FACTOR", "1.5");
#endif
#endif

  QApplication a(argc, argv);
  a.setApplicationName("GManager");

  // 单例：已有程序在运行时聚焦它并退出，避免出现第二个窗口
  if (notifyRunningInstance()) {
    return 0;
  }

  eApp->init();
  // ElaLog::getInstance()->initMessageLog(true);
  GManager::MainWindow w;
  w.show();

  // 本进程作为单实例服务端，收到新实例的通知时把主窗口拉回前台
  QLocalServer server;
  QLocalServer::removeServer(kSingleInstanceKey);
  if (server.listen(kSingleInstanceKey)) {
    QObject::connect(
        &server, &QLocalServer::newConnection, &a, [&server, &w]() {
          while (QLocalSocket *client = server.nextPendingConnection()) {
            QObject::connect(client, &QLocalSocket::disconnected, client,
                             &QLocalSocket::deleteLater);
            // 不主动断开：新实例写完通知后会自行断开
            focusMainWindow(&w);
          }
        });
  }

#ifdef Q_OS_WIN
  HWND handle = FindWindowA(NULL, "ElaWidgetTool");
  if (handle != NULL) {
    SetWindowDisplayAffinity(handle, WDA_EXCLUDEFROMCAPTURE);
  }
#endif

  return a.exec();
}
