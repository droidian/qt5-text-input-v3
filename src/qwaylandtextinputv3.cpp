// Copyright (C) 2021 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "qwaylandtextinputv3_p.h"

#include <QtWaylandClient/private/qwaylandwindow_p.h>
#include <QtWaylandClient/private/qwaylandscreen_p.h>
#include <QtWaylandClient/private/qwaylandinputdevice_p.h>
#include "qwaylandinputmethodeventbuilder_p.h"

#include <QtCore/qloggingcategory.h>
#include <QtGui/qguiapplication.h>
#include <QtGui/private/qhighdpiscaling_p.h>
#include <QtGui/qevent.h>
#include <QtGui/qwindow.h>
#include <QTextCharFormat>

QT_BEGIN_NAMESPACE

Q_LOGGING_CATEGORY(qLcQpaWaylandTextInput, "qt.qpa.wayland.textinput")

namespace QtWaylandClient
{

QWaylandTextInputv3Manager::QWaylandTextInputv3Manager()
	: QWaylandClientExtensionTemplate(1)
{
	connect(this, &QWaylandClientExtension::activeChanged, this,
		&QWaylandTextInputv3Manager::onActiveChanged);
}

QWaylandTextInputv3Manager::~QWaylandTextInputv3Manager()
{
	destroy();
}

QWaylandTextInputv3::QWaylandTextInputv3(QWaylandTextInputv3Manager *manager,
					 struct ::zwp_text_input_v3 *object)
	: QWaylandClientExtensionTemplate(1)
	, QtWayland::zwp_text_input_v3(object)
	, m_manager(manager)
{
}

QWaylandTextInputv3::~QWaylandTextInputv3()
{
	destroy();
}

void QWaylandTextInputv3Manager::onActiveChanged()
{
	if (!isActive())
		return;

	QList<QWaylandInputDevice *> inputDevices =
		static_cast<QWaylandScreen *>(qApp->screens().first()->handle())
			->display()
			->inputDevices();
	for (auto dev : inputDevices) {
		m_inputs << new QWaylandTextInputv3(
			this, get_text_input(dev->wl_seat()));
	}
}

namespace
{
const Qt::InputMethodQueries supportedQueries3 =
	Qt::ImEnabled | Qt::ImSurroundingText | Qt::ImCursorPosition |
	Qt::ImAnchorPosition | Qt::ImHints | Qt::ImCursorRectangle;
}

void QWaylandTextInputv3::zwp_text_input_v3_enter(struct ::wl_surface *surface)
{
	m_manager->m_pendingPreeditString.clear();
	m_manager->m_pendingCommitString.clear();
	m_manager->m_pendingDeleteBeforeText = 0;
	m_manager->m_pendingDeleteAfterText = 0;

	m_manager->updateState(supportedQueries3,
			       m_manager->update_state_enter);
}

void QWaylandTextInputv3::zwp_text_input_v3_leave(struct ::wl_surface *surface)
{
	qCDebug(qLcQpaWaylandTextInput) << Q_FUNC_INFO;

	m_manager->m_currentPreeditString.clear();
	m_manager->m_currentSerial = 0U;

	disable();
	qCDebug(qLcQpaWaylandTextInput) << Q_FUNC_INFO << "Done";
}

void QWaylandTextInputv3::zwp_text_input_v3_preedit_string(const QString &text,
							   int32_t cursorBegin,
							   int32_t cursorEnd)
{
	qCDebug(qLcQpaWaylandTextInput)
		<< Q_FUNC_INFO << text << cursorBegin << cursorEnd;

	if (!QGuiApplication::focusObject())
		return;

	m_manager->m_pendingPreeditString.text = text;
	m_manager->m_pendingPreeditString.cursorBegin = cursorBegin;
	m_manager->m_pendingPreeditString.cursorEnd = cursorEnd;
}

void QWaylandTextInputv3::zwp_text_input_v3_commit_string(const QString &text)
{
	qCDebug(qLcQpaWaylandTextInput) << Q_FUNC_INFO << text;

	if (!QGuiApplication::focusObject())
		return;

	m_manager->m_pendingCommitString = text;
}

void QWaylandTextInputv3::zwp_text_input_v3_delete_surrounding_text(
	uint32_t beforeText, uint32_t afterText)
{
	qCDebug(qLcQpaWaylandTextInput)
		<< Q_FUNC_INFO << beforeText << afterText;

	if (!QGuiApplication::focusObject())
		return;

	m_manager->m_pendingDeleteBeforeText =
		QWaylandInputMethodEventBuilder::indexFromWayland(
			m_manager->m_surroundingText, beforeText);
	m_manager->m_pendingDeleteAfterText =
		QWaylandInputMethodEventBuilder::indexFromWayland(
			m_manager->m_surroundingText, afterText);
}

void QWaylandTextInputv3::zwp_text_input_v3_done(uint32_t serial)
{
	qCDebug(qLcQpaWaylandTextInput) << Q_FUNC_INFO << "with serial"
					<< serial << m_manager->m_currentSerial;

	// This is a case of double click.
	// text_input_v3 will ignore this done signal and just keep the selection of the clicked word.
	if (m_manager->m_cursorPos != m_manager->m_anchorPos &&
	    (m_manager->m_pendingDeleteBeforeText != 0 ||
	     m_manager->m_pendingDeleteAfterText != 0)) {
		qCDebug(qLcQpaWaylandTextInput) << Q_FUNC_INFO << "Ignore done";
		m_manager->m_pendingDeleteBeforeText = 0;
		m_manager->m_pendingDeleteAfterText = 0;
		m_manager->m_pendingPreeditString.clear();
		m_manager->m_pendingCommitString.clear();
		return;
	}

	QObject *focusObject = QGuiApplication::focusObject();
	if (!focusObject)
		return;

	qCDebug(qLcQpaWaylandTextInput)
		<< Q_FUNC_INFO << "PREEDIT"
		<< m_manager->m_pendingPreeditString.text
		<< m_manager->m_pendingPreeditString.cursorBegin;

	QList<QInputMethodEvent::Attribute> attributes;
	{
		if (m_manager->m_pendingPreeditString.cursorBegin != -1 ||
		    m_manager->m_pendingPreeditString.cursorEnd != -1) {
			// Current supported cursor shape is just line.
			// It means, cursorEnd and cursorBegin are the same.
			QInputMethodEvent::Attribute attribute1(
				QInputMethodEvent::Cursor,
				m_manager->m_pendingPreeditString.text.length(),
				1);
			attributes.append(attribute1);
		}

		// only use single underline style for now
		QTextCharFormat format;
		format.setFontUnderline(true);
		format.setUnderlineStyle(QTextCharFormat::SingleUnderline);
		QInputMethodEvent::Attribute attribute2(
			QInputMethodEvent::TextFormat, 0,
			m_manager->m_pendingPreeditString.text.length(),
			format);
		attributes.append(attribute2);
	}
	QInputMethodEvent event(m_manager->m_pendingPreeditString.text,
				attributes);

	qCDebug(qLcQpaWaylandTextInput) << Q_FUNC_INFO << "DELETE"
					<< m_manager->m_pendingDeleteBeforeText
					<< m_manager->m_pendingDeleteAfterText;
	qCDebug(qLcQpaWaylandTextInput)
		<< Q_FUNC_INFO << "COMMIT" << m_manager->m_pendingCommitString;

	// A workaround for reselection
	// It will disable redundant commit after reselection
	if (m_manager->m_pendingDeleteBeforeText != 0 ||
	    m_manager->m_pendingDeleteAfterText != 0)
		m_manager->m_condReselection = true;

	event.setCommitString(m_manager->m_pendingCommitString,
			      -m_manager->m_pendingDeleteBeforeText,
			      m_manager->m_pendingDeleteBeforeText +
				      m_manager->m_pendingDeleteAfterText);
	m_manager->m_currentPreeditString = m_manager->m_pendingPreeditString;
	m_manager->m_pendingPreeditString.clear();
	m_manager->m_pendingCommitString.clear();
	m_manager->m_pendingDeleteBeforeText = 0;
	m_manager->m_pendingDeleteAfterText = 0;
	QCoreApplication::sendEvent(focusObject, &event);

	if (serial == m_manager->m_currentSerial)
		m_manager->updateState(supportedQueries3,
				       m_manager->update_state_full);
}

void QWaylandTextInputv3Manager::reset()
{
	qCDebug(qLcQpaWaylandTextInput) << Q_FUNC_INFO;

	m_pendingPreeditString.clear();
}

void QWaylandTextInputv3Manager::commit()
{
	m_currentSerial = (m_currentSerial < UINT_MAX) ? m_currentSerial + 1U :
							 0U;

	qCDebug(qLcQpaWaylandTextInput)
		<< Q_FUNC_INFO << "with serial" << m_currentSerial;
	for (auto input : m_inputs)
		input->commit();
}

void QWaylandTextInputv3Manager::updateState(Qt::InputMethodQueries queries,
					     uint32_t flags)
{
	qCDebug(qLcQpaWaylandTextInput) << Q_FUNC_INFO << queries << flags;

	if (!QGuiApplication::focusObject())
		return;

	if (!QGuiApplication::focusWindow() ||
	    !QGuiApplication::focusWindow()->handle())
		return;

	auto *window = static_cast<QWaylandWindow *>(
		QGuiApplication::focusWindow()->handle());
	auto *surface = window->wlSurface();
	if (!surface)
		return;

	queries &= supportedQueries3;
	bool needsCommit = false;

	QInputMethodQueryEvent event(queries);
	QCoreApplication::sendEvent(QGuiApplication::focusObject(), &event);

	// For some reason, a query for Qt::ImSurroundingText gives an empty string even though it is not.
	if (!(queries & Qt::ImSurroundingText) &&
	    event.value(Qt::ImSurroundingText).toString().isEmpty()) {
		return;
	}

	if (queries & Qt::ImCursorRectangle) {
		const QRect &cRect =
			event.value(Qt::ImCursorRectangle).toRect();
		const QRect &windowRect = QGuiApplication::inputMethod()
						  ->inputItemTransform()
						  .mapRect(cRect);
		const QRect &nativeRect = QHighDpi::toNativePixels(
			windowRect, QGuiApplication::focusWindow());
		const QMargins margins = window->frameMargins();
		const QRect &surfaceRect =
			nativeRect.translated(margins.left(), margins.top());
		if (surfaceRect != m_cursorRect) {
			for (auto input : m_inputs)
				input->set_cursor_rectangle(
					surfaceRect.x(), surfaceRect.y(),
					surfaceRect.width(),
					surfaceRect.height());
			m_cursorRect = surfaceRect;
			needsCommit = true;
		}
	}

	if ((queries & Qt::ImSurroundingText) ||
	    (queries & Qt::ImCursorPosition) ||
	    (queries & Qt::ImAnchorPosition)) {
		QString text = event.value(Qt::ImSurroundingText).toString();
		int cursor = event.value(Qt::ImCursorPosition).toInt();
		int anchor = event.value(Qt::ImAnchorPosition).toInt();

		qCDebug(qLcQpaWaylandTextInput)
			<< "Orginal surrounding_text from InputMethodQuery: "
			<< text << cursor << anchor;

		// Make sure text is not too big
		// surround_text cannot exceed 4000byte in wayland protocol
		// The worst case will be supposed here.
		const int MAX_MESSAGE_SIZE = 4000;

		if (text.toUtf8().size() > MAX_MESSAGE_SIZE) {
			const int selectionStart =
				QWaylandInputMethodEventBuilder::indexToWayland(
					text, qMin(cursor, anchor));
			const int selectionEnd =
				QWaylandInputMethodEventBuilder::indexToWayland(
					text, qMax(cursor, anchor));
			const int selectionLength =
				selectionEnd - selectionStart;
			// If selection is bigger than 4000 byte, it is fixed to 4000 byte.
			// anchor will be moved in the 4000 byte boundary.
			if (selectionLength > MAX_MESSAGE_SIZE) {
				if (anchor > cursor) {
					const int length = MAX_MESSAGE_SIZE;
					anchor =
						QWaylandInputMethodEventBuilder::
							trimmedIndexFromWayland(
								text, length,
								cursor);
					anchor -= cursor;
					text = text.mid(cursor, anchor);
					cursor = 0;
				} else {
					const int length = -MAX_MESSAGE_SIZE;
					anchor =
						QWaylandInputMethodEventBuilder::
							trimmedIndexFromWayland(
								text, length,
								cursor);
					cursor -= anchor;
					text = text.mid(anchor, cursor);
					anchor = 0;
				}
			} else {
				const int offset =
					(MAX_MESSAGE_SIZE - selectionLength) /
					2;

				int textStart =
					QWaylandInputMethodEventBuilder::
						trimmedIndexFromWayland(
							text, -offset,
							qMin(cursor, anchor));
				int textEnd = QWaylandInputMethodEventBuilder::
					trimmedIndexFromWayland(
						text, MAX_MESSAGE_SIZE,
						textStart);

				anchor -= textStart;
				cursor -= textStart;
				text = text.mid(textStart, textEnd - textStart);
			}
		}
		qCDebug(qLcQpaWaylandTextInput)
			<< "Modified surrounding_text: " << text << cursor
			<< anchor;

		const int cursorPos =
			QWaylandInputMethodEventBuilder::indexToWayland(text,
									cursor);
		const int anchorPos =
			QWaylandInputMethodEventBuilder::indexToWayland(text,
									anchor);

		if (m_surroundingText != text || m_cursorPos != cursorPos ||
		    m_anchorPos != anchorPos) {
			qCDebug(qLcQpaWaylandTextInput)
				<< "Current surrounding_text: "
				<< m_surroundingText << m_cursorPos
				<< m_anchorPos;
			qCDebug(qLcQpaWaylandTextInput)
				<< "New surrounding_text: " << text << cursorPos
				<< anchorPos;

			for (auto input : m_inputs)
				input->set_surrounding_text(text, cursorPos,
							    anchorPos);

			// A workaround in the case of reselection
			// It will work when re-clicking a preedit text
			if (m_condReselection) {
				qCDebug(qLcQpaWaylandTextInput)
					<< "\"commit\" is disabled when Reselection by changing focus";
				m_condReselection = false;
				needsCommit = false;
			}

			m_surroundingText = text;
			m_cursorPos = cursorPos;
			m_anchorPos = anchorPos;
			m_cursor = cursor;
		}
	}

	if (queries & Qt::ImHints) {
		QWaylandInputMethodContentType contentType =
			QWaylandInputMethodContentType::convertV3(
				static_cast<Qt::InputMethodHints>(
					event.value(Qt::ImHints).toInt()));
		qCDebug(qLcQpaWaylandTextInput)
			<< m_contentHint << contentType.hint;
		qCDebug(qLcQpaWaylandTextInput)
			<< m_contentPurpose << contentType.purpose;

		if (m_contentHint != contentType.hint ||
		    m_contentPurpose != contentType.purpose) {
			qCDebug(qLcQpaWaylandTextInput)
				<< "set_content_type: " << contentType.hint
				<< contentType.purpose;
			for (auto input : m_inputs)
				input->set_content_type(contentType.hint,
							contentType.purpose);

			m_contentHint = contentType.hint;
			m_contentPurpose = contentType.purpose;
			needsCommit = true;
		}
	}

	if (needsCommit &&
	    (flags == update_state_change || flags == update_state_enter))
		commit();
}

void QWaylandTextInputv3Manager::setCursorInsidePreedit(int cursor)
{
	Q_UNUSED(cursor);
}

bool QWaylandTextInputv3Manager::isInputPanelVisible() const
{
	return m_panelVisible;
}

QRectF QWaylandTextInputv3Manager::keyboardRect() const
{
	qCDebug(qLcQpaWaylandTextInput) << Q_FUNC_INFO;
	return m_cursorRect;
}

QLocale QWaylandTextInputv3Manager::locale() const
{
	return QLocale();
}

Qt::LayoutDirection QWaylandTextInputv3Manager::inputDirection() const
{
	return Qt::LeftToRight;
}

void QWaylandTextInputv3Manager::showInputPanel()
{
	for (auto input : m_inputs) {
		input->enable();
		input->commit();
	}
	m_panelVisible = true;
}

void QWaylandTextInputv3Manager::hideInputPanel()
{
	for (auto input : m_inputs) {
		input->disable();
		input->commit();
	}
	m_panelVisible = false;
}

}

QT_END_NAMESPACE
