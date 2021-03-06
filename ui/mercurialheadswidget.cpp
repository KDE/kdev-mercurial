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

#include "mercurialheadswidget.h"

#include <vcs/vcsjob.h>
#include <vcs/vcsevent.h>

#include <interfaces/icore.h>
#include <interfaces/iruncontroller.h>

#include <models/mercurialheadsmodel.h>
#include <mercurialplugin.h>

using namespace KDevelop;

MercurialHeadsWidget::MercurialHeadsWidget(MercurialPlugin *plugin, const QUrl &url)
    : QDialog(), m_ui(new Ui::MercurialHeadsWidget), m_plugin(plugin), m_url(url)
{
    m_ui->setupUi(this);
    m_headsModel = new MercurialHeadsModel(plugin, url, this);
    m_ui->headsTableView->setModel(static_cast<QAbstractItemModel *>(m_headsModel));

    connect(m_ui->checkoutPushButton, &QPushButton::clicked, this,  &MercurialHeadsWidget::checkoutRequested);
    connect(m_ui->mergePushButton, &QPushButton::clicked, this, &MercurialHeadsWidget::mergeRequested);

    setWindowTitle(i18n("Mercurial Heads (%1)", m_url.toLocalFile()));

    updateModel();
}

void MercurialHeadsWidget::updateModel()
{
    m_headsModel->update();
}

void MercurialHeadsWidget::checkoutRequested()
{
    const QModelIndex &selection = m_ui->headsTableView->currentIndex();
    if (!selection.isValid()) {
        return;
    }

    VcsJob *job = m_plugin->checkoutHead(m_url, m_headsModel->eventForIndex(selection).revision());
    connect(job, &VcsJob::resultsReady, this, &MercurialHeadsWidget::updateModel);
    ICore::self()->runController()->registerJob(job);
}

void MercurialHeadsWidget::mergeRequested()
{
    const QModelIndex &selection = m_ui->headsTableView->currentIndex();
    if (!selection.isValid()) {
        return;
    }

    VcsJob *job = m_plugin->mergeWith(m_url, m_headsModel->eventForIndex(selection).revision());
    connect(job, &VcsJob::resultsReady, this, &MercurialHeadsWidget::updateModel);
    ICore::self()->runController()->registerJob(job);
}
