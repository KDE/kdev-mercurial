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

#ifndef MERCURIALQUEUESERIESMODEL_H
#define MERCURIALQUEUESERIESMODEL_H

#include <QModelIndex>
#include <QStringList>

class QUrl;
class MercurialPlugin;

class MercurialQueueSeriesModel : public QAbstractListModel
{
    Q_OBJECT

public:
    explicit MercurialQueueSeriesModel(MercurialPlugin *plugin, const QUrl &localLocation, QObject *parent = 0)
        : QAbstractListModel(parent), m_repoLocation(localLocation), m_plugin(plugin) {
        update();
    };

    void update();

    virtual QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const;
    virtual int rowCount(const QModelIndex &parent = QModelIndex()) const;

private:
    const QUrl &m_repoLocation;
    MercurialPlugin *m_plugin;
    QStringList m_appliedPatches, m_unappliedPatches;
};

#endif // MERCURIALQUEUESERIESMODEL_H
