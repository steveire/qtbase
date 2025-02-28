/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtCore module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl-3.0.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or (at your option) the GNU General
** Public license version 3 or any later version approved by the KDE Free
** Qt Foundation. The licenses are as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL2 and LICENSE.GPL3
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-2.0.html and
** https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "qwindowspipewriter_p.h"

QT_BEGIN_NAMESPACE

#ifndef QT_NO_THREAD

QWindowsPipeWriter::QWindowsPipeWriter(HANDLE pipe, QObject * parent)
    : QThread(parent),
      writePipe(INVALID_HANDLE_VALUE),
      quitNow(false),
      hasWritten(false)
{
    DuplicateHandle(GetCurrentProcess(), pipe, GetCurrentProcess(),
                         &writePipe, 0, FALSE, DUPLICATE_SAME_ACCESS);
}

QWindowsPipeWriter::~QWindowsPipeWriter()
{
    lock.lock();
    quitNow = true;
    waitCondition.wakeOne();
    lock.unlock();
    if (!wait(30000))
        terminate();
    CloseHandle(writePipe);
}

bool QWindowsPipeWriter::waitForWrite(int msecs)
{
    QMutexLocker locker(&lock);
    bool hadWritten = hasWritten;
    hasWritten = false;
    if (hadWritten)
        return true;
    if (!waitCondition.wait(&lock, msecs))
        return false;
    hadWritten = hasWritten;
    hasWritten = false;
    return hadWritten;
}

qint64 QWindowsPipeWriter::write(const char *ptr, qint64 maxlen)
{
    if (!isRunning())
        return -1;

    QMutexLocker locker(&lock);
    data.append(ptr, maxlen);
    waitCondition.wakeOne();
    return maxlen;
}

class QPipeWriterOverlapped
{
public:
    QPipeWriterOverlapped()
    {
        overlapped.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
    }

    ~QPipeWriterOverlapped()
    {
        CloseHandle(overlapped.hEvent);
    }

    void prepare()
    {
        const HANDLE hEvent = overlapped.hEvent;
        ZeroMemory(&overlapped, sizeof overlapped);
        overlapped.hEvent = hEvent;
    }

    OVERLAPPED *operator&()
    {
        return &overlapped;
    }

private:
    OVERLAPPED overlapped;
};

void QWindowsPipeWriter::run()
{
    QPipeWriterOverlapped overl;
    forever {
        lock.lock();
        while(data.isEmpty() && (!quitNow)) {
            waitCondition.wakeOne();
            waitCondition.wait(&lock);
        }

        if (quitNow) {
            lock.unlock();
            quitNow = false;
            break;
        }

        QByteArray copy = data;

        lock.unlock();

        const char *ptrData = copy.data();
        qint64 maxlen = copy.size();
        qint64 totalWritten = 0;
        overl.prepare();
        while ((!quitNow) && totalWritten < maxlen) {
            DWORD written = 0;
            if (!WriteFile(writePipe, ptrData + totalWritten,
                           maxlen - totalWritten, &written, &overl)) {
                const DWORD writeError = GetLastError();
                if (writeError == 0xE8/*NT_STATUS_INVALID_USER_BUFFER*/) {
                    // give the os a rest
                    msleep(100);
                    continue;
                }
                if (writeError != ERROR_IO_PENDING) {
                    qErrnoWarning(writeError, "QWindowsPipeWriter: async WriteFile failed.");
                    return;
                }
                if (!GetOverlappedResult(writePipe, &overl, &written, TRUE)) {
                    qErrnoWarning(GetLastError(), "QWindowsPipeWriter: GetOverlappedResult failed.");
                    return;
                }
            }
            totalWritten += written;
#if defined QPIPEWRITER_DEBUG
            qDebug("QWindowsPipeWriter::run() wrote %d %d/%d bytes",
                   written, int(totalWritten), int(maxlen));
#endif
            lock.lock();
            data.remove(0, written);
            hasWritten = true;
            lock.unlock();
        }
        emit bytesWritten(totalWritten);
        emit canWrite();
    }
}

#endif //QT_NO_THREAD

QT_END_NAMESPACE
