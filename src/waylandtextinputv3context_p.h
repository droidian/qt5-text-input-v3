// Copyright (C) 2016 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#ifndef WAYLANDTEXTINPUTV3CONTEXT_H
#define WAYLANDTEXTINPUTV3CONTEXT_H

//
//  W A R N I N G
//  -------------
//
// This file is not part of the Qt API.  It exists purely as an
// implementation detail.  This header file may change from version to
// version without notice, or even be removed.
//
// We mean it.
//

#include <qpa/qplatforminputcontext.h>

#include <QPointer>

#include <QtWaylandClient/private/qwaylandinputcontext_p.h>
#include <QtWaylandClient/private/qtwaylandclientglobal_p.h>

#include "qwaylandtextinputv3_p.h"

struct wl_callback;
struct wl_callback_listener;

QT_BEGIN_NAMESPACE

namespace QtWaylandClient
{

class QWaylandDisplay;

class WaylandTextInputV3Context : public QPlatformInputContext {
	Q_OBJECT
    public:
	explicit WaylandTextInputV3Context();
	~WaylandTextInputV3Context() override;

	bool isValid() const override;

	void reset() override;
	void commit() override;
	void update(Qt::InputMethodQueries) override;

	void invokeAction(QInputMethod::Action, int cursorPosition) override;

	void showInputPanel() override;
	void hideInputPanel() override;
	bool isInputPanelVisible() const override;
	QRectF keyboardRect() const override;

	QLocale locale() const override;
	Qt::LayoutDirection inputDirection() const override;

	void setFocusObject(QObject *object) override;

    private:
	QWaylandTextInputv3Manager *textInput() const;

	QPointer<QWindow> mCurrentWindow;
	QWaylandTextInputv3Manager *mTextInput = nullptr;

    private slots:
	void onScreenAdded(QScreen *screen);
};

}

QT_END_NAMESPACE

#endif // WAYLANDTEXTINPUTV3CONTEXT_H
