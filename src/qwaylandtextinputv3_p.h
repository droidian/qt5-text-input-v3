// Copyright (C) 2021 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#ifndef QWAYLANDTEXTINPUTV3_P_H
#define QWAYLANDTEXTINPUTV3_P_H

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
#include <QtWaylandClient/QWaylandClientExtensionTemplate>
#include "qwayland-text-input-unstable-v3.h"
#include "qwaylandinputmethodeventbuilder_p.h"
#include <QLoggingCategory>

struct wl_callback;
struct wl_callback_listener;

QT_BEGIN_NAMESPACE

Q_DECLARE_LOGGING_CATEGORY(qLcQpaWaylandTextInput)

namespace QtWaylandClient
{

class QWaylandTextInputv3;

class Q_WAYLAND_CLIENT_EXPORT QWaylandTextInputv3Manager
	: public QWaylandClientExtensionTemplate<QWaylandTextInputv3Manager>,
	  public QtWayland::zwp_text_input_manager_v3 {
	Q_OBJECT
	friend class QWaylandTextInputv3;

    public:
	enum TextInputState {
		update_state_change = 0, // updated state because it changed
		update_state_full =
			1, // full state after enter or input_method_changed event
		update_state_reset = 2, // full state after reset
		update_state_enter =
			3, // full state after switching focus to a different widget on client side
	};

	QWaylandTextInputv3Manager();
	~QWaylandTextInputv3Manager() override;

	void reset();
	void commit();
	void updateState(Qt::InputMethodQueries queries, uint32_t flags);
	// TODO: not supported yet
	void setCursorInsidePreedit(int cursor);

	bool isInputPanelVisible() const;
	QRectF keyboardRect() const;

	QLocale locale() const;
	Qt::LayoutDirection inputDirection() const;

	void showInputPanel();
	void hideInputPanel();

	void enableSurface(::wl_surface *surface)
	{
		showInputPanel();
	}
	void disableSurface(::wl_surface *surface)
	{
		hideInputPanel();
	}

    protected:
	QList<QWaylandTextInputv3 *> m_inputs;
	QWaylandInputMethodEventBuilder m_builder;

	struct PreeditInfo {
		QString text;
		int cursorBegin = 0;
		int cursorEnd = 0;

		void clear()
		{
			text.clear();
			cursorBegin = 0;
			cursorEnd = 0;
		}
	};

	PreeditInfo m_pendingPreeditString;
	PreeditInfo m_currentPreeditString;
	QString m_pendingCommitString;
	uint m_pendingDeleteBeforeText = 0;
	uint m_pendingDeleteAfterText = 0;

	QString m_surroundingText;
	int m_cursor; // cursor position in QString
	int m_cursorPos; // cursor position in wayland index
	int m_anchorPos; // anchor position in wayland index
	uint32_t m_contentHint = 0;
	uint32_t m_contentPurpose = 0;
	QRect m_cursorRect;

	uint m_currentSerial = 0;

	bool m_condReselection = false;
	bool m_panelVisible = false;

    private slots:
	void onActiveChanged();
};

class Q_WAYLAND_CLIENT_EXPORT QWaylandTextInputv3
	: public QWaylandClientExtensionTemplate<QWaylandTextInputv3>,
	  public QtWayland::zwp_text_input_v3 {
	Q_OBJECT
    public:
	QWaylandTextInputv3(QWaylandTextInputv3Manager *m_manager,
			    struct ::zwp_text_input_v3 *object);
	~QWaylandTextInputv3() override;

    protected:
	void zwp_text_input_v3_enter(struct ::wl_surface *surface) override;
	void zwp_text_input_v3_leave(struct ::wl_surface *surface) override;
	void zwp_text_input_v3_preedit_string(const QString &text,
					      int32_t cursor_begin,
					      int32_t cursor_end) override;
	void zwp_text_input_v3_commit_string(const QString &text) override;
	void zwp_text_input_v3_delete_surrounding_text(
		uint32_t before_length, uint32_t after_length) override;
	void zwp_text_input_v3_done(uint32_t serial) override;

    private:
	QWaylandTextInputv3Manager *m_manager;
};
}

QT_END_NAMESPACE

#endif // QWAYLANDTEXTINPUTV3_P_H
