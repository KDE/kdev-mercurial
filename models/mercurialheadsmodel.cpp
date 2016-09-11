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

#include "mercurialheadsmodel.h"

#include <QIcon>

#include <vcs/vcsevent.h>
#include <vcs/vcsrevision.h>

using namespace KDevelop;

MercurialHeadsModel::MercurialHeadsModel(IBasicVersionControl *iface, const KDevelop::VcsRevision &rev, const QUrl &url, QObject *parent)
    : VcsEventModel(iface, rev, url, parent)
{

}

QVariant MercurialHeadsModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation == Qt::Vertical && role == Qt::DecorationRole) {
        const VcsRevision &rev = eventForIndex(index(section, 0)).revision();
        if (m_currentHeads.contains(rev)) {
            return QVariant(QIcon("arrow-right"));
        } else {
            return QVariant();
        }
    } else {
        return VcsEventModel::headerData(section, orientation, role);
    }
}
