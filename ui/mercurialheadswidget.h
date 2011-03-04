/***************************************************************************
 *   Copyright 2011 Andrey Batyiev <batyiev@gmail.com>                     *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or         *
 *   modify it under the terms of the GNU General Public License as        *
 *   published by the Free Software Foundation; either version 2 of        *
 *   the License or (at your option) version 3 or any later version        *
 *   accepted by the membership of KDE e.V. (or its successor approved     *
 *   by the membership of KDE e.V.), which shall act as a proxy            *
 *   defined in Section 14 of version 3 of the license.                    *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program.  If not, see <http://www.gnu.org/licenses/>. *
 ***************************************************************************/

#ifndef MERCURIALHEADSWIDGET_H
#define MERCURIALHEADSWIDGET_H

#include <QtGui/QWidget>

#include <vcs/vcsrevision.h>

#include "ui_mercurialheadswidget.h"

class MercurialPlugin;

namespace KDevelop {
class VcsJob;
}

class MercurialHeadsModel;

class MercurialHeadsWidget : public QDialog
{
    Q_OBJECT

public:
    explicit MercurialHeadsWidget(MercurialPlugin *plugin, const KUrl &url);

private slots:
    void updateModel();

    void headsReceived(KDevelop::VcsJob *job);
    void identifyReceived(KDevelop::VcsJob *job);

    void checkoutRequested();

private:
    Ui::MercurialHeadsWidget *m_ui;
    MercurialHeadsModel *m_headsModel;
    MercurialPlugin *m_plugin;
    const KUrl &m_url;
    KDevelop::VcsRevision m_currentHead;
};

#endif // MERCURIALHEADSWIDGET_H
