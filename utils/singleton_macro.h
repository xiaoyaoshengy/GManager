#ifndef SINGLETON_MACRO_H
#define SINGLETON_MACRO_H

#include <QMutex>
#include <QScopedPointer>

#define SINGLETON_HEADER(CLASS)                                                \
public:                                                                        \
  static CLASS *getInstance();                                                 \
                                                                               \
private:                                                                       \
  explicit CLASS(QObject *parent = nullptr);                                   \
  ~CLASS();                                                                    \
  CLASS(const CLASS &) = delete;                                               \
  CLASS &operator=(const CLASS &) = delete;                                    \
                                                                               \
  friend struct QScopedPointerDeleter<CLASS>;                                  \
                                                                               \
  static QMutex mtx;                                                           \
  static QScopedPointer<CLASS> inst;

#define SINGLETON_SOURCE(CLASS)                                                \
  QMutex CLASS::mtx;                                                           \
  QScopedPointer<CLASS> CLASS::inst;                                           \
                                                                               \
  CLASS *CLASS::getInstance() {                                                \
    if (inst.isNull()) {                                                       \
      mtx.lock();                                                              \
      if (inst.isNull()) {                                                     \
        inst.reset(new CLASS());                                               \
      }                                                                        \
      mtx.unlock();                                                            \
    }                                                                          \
    return inst.data();                                                        \
  }

#endif // !SINGLETON_MACRO_H
