# -*- coding: utf-8 -*-
#
#  Copyright 2011 Andrey Batyiev <batyiev@gmail.com>
#
#  This program is free software; you can redistribute it and/or
#  modify it under the terms of the GNU General Public License as
#  published by the Free Software Foundation; either version 2 of
#  the License or (at your option) version 3 or any later version
#  accepted by the membership of KDE e.V. (or its successor approved
#  by the membership of KDE e.V.), which shall act as a proxy
#  defined in Section 14 of version 3 of the license.
#
#  This program is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU General Public License for more details.
#
#  You should have received a copy of the GNU General Public License
#  along with this program.  If not, see <http://www.gnu.org/licenses/>.
#

"""kdevplatform integration"""

from mercurial import commands, ui

def resolve_and_status(ui, repo, *pats, **opts):
    commands.resolve(ui, repo, *pats, list=True)
    ui.write('%\n')  # separator
    commands.status(ui, repo, *pats, all=True)

def allbranches(ui, repo, *pats, **opts):
    branches = repo.branchtags().keys()
    current_branch = repo.dirstate.branch()
    if current_branch not in branches:
        branches.append(current_branch)
    for branch in branches:
        ui.write("%s\n" % branch)

cmdtable = {
               "resolveandstatus" : (resolve_and_status, [], 'Print all status info (including resolution state)'),
               "allbranches" : (allbranches, [], 'Print all branches (including uncommited new one)'),
           }
