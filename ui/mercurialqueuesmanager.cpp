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

#include "mercurialqueuesmanager.h"
#include "ui_mercurialqueuesmanager.h"
#include <models/mercurialqueueseriesmodel.h>
#include <vcs/vcsjob.h>
#include <mercurialplugin.h>

using namespace KDevelop;

MercurialQueuesManager::MercurialQueuesManager(MercurialPlugin *plugin, const QUrl &localLocation)
    : QWidget(), m_plugin(plugin), repoLocation(localLocation), m_ui(new Ui::MercurialManagerWidget)
{
    m_ui->setupUi(this);
    m_model = new MercurialQueueSeriesModel(m_plugin, repoLocation, m_ui->patchesListView);
    m_ui->patchesListView->setModel(m_model);

    connect(m_ui->pushPushButton, SIGNAL(clicked(bool)), this, SLOT(executePush()));
    connect(m_ui->pushAllPushButton, SIGNAL(clicked(bool)), this, SLOT(executePushAll()));
    connect(m_ui->popPushButton, SIGNAL(clicked(bool)), this, SLOT(executePop()));
    connect(m_ui->popAllPushButton, SIGNAL(clicked(bool)), this, SLOT(executePopAll()));
}

void MercurialQueuesManager::executePush()
{
    QScopedPointer<VcsJob> job(m_plugin->mqPush(repoLocation));
    job->exec();
    m_model->update();
}

void MercurialQueuesManager::executePushAll()
{
    QScopedPointer<VcsJob> job(m_plugin->mqPushAll(repoLocation));
    job->exec();
    m_model->update();
}

void MercurialQueuesManager::executePop()
{
    QScopedPointer<VcsJob> job(m_plugin->mqPop(repoLocation));
    job->exec();
    m_model->update();
}

void MercurialQueuesManager::executePopAll()
{
    QScopedPointer<VcsJob> job(m_plugin->mqPopAll(repoLocation));
    job->exec();
    m_model->update();
}
