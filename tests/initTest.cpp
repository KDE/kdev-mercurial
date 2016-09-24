/***************************************************************************
 *   This file was taken from KDevelop's git plugin                        *
 *   Copyright 2008 Evgeniy Ivanov <powerfox@kde.ru>                       *
 *                                                                         *
 *   Adapted for Mercurial                                                 *
 *   Copyright 2009 Fabian Wiesel <fabian.wiesel@fu-berlin.de>             *
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

#include "initTest.h"

#include <QtTest/QtTest>
#include <tests/testcore.h>
#include <tests/autotestshell.h>

#include <QUrl>

#include <KIO/DeleteJob>

#include <vcs/dvcs/dvcsjob.h>
#include "../mercurialplugin.h"
#include "debug.h"

const QString tempDir = QDir::tempPath();
const QString MercurialTestDir1("kdevMercurial_testdir");
const QString mercurialTest_BaseDir(tempDir + "/kdevMercurial_testdir/");
const QString mercurialTest_BaseDir2(tempDir + "/kdevMercurial_testdir2/");
const QString mercurialRepo(mercurialTest_BaseDir + ".hg");
const QString mercurialSrcDir(mercurialTest_BaseDir + "src/");
const QString mercurialTest_FileName("testfile");
const QString mercurialTest_FileName2("foo");
const QString mercurialTest_FileName3("bar");

using namespace KDevelop;

void MercurialInitTest::initTestCase()
{
    AutoTestShell::init();
    m_testCore = new KDevelop::TestCore();
    m_testCore->initialize(KDevelop::Core::NoUi);
    // m_testCore->initialize(KDevelop::Core::Default);

    m_proxy = new MercurialPlugin(m_testCore);
    removeTempDirs();

    // Now create the basic directory structure
    QDir tmpdir(tempDir);
    tmpdir.mkdir(mercurialTest_BaseDir);
    tmpdir.mkdir(mercurialSrcDir);
    tmpdir.mkdir(mercurialTest_BaseDir2);
}

void MercurialInitTest::cleanupTestCase()
{
    delete m_proxy;

    if (QFileInfo(mercurialTest_BaseDir).exists())
        KIO::del(QUrl::fromLocalFile(mercurialTest_BaseDir))->exec();

    if (QFileInfo(mercurialTest_BaseDir2).exists())
        KIO::del(QUrl::fromLocalFile(mercurialTest_BaseDir2))->exec();
}

void MercurialInitTest::repoInit()
{
    mercurialDebug() << "Trying to init repo";
    // make job that creates the local repository
    VcsJob *j = m_proxy->init(QUrl::fromLocalFile(mercurialTest_BaseDir));
    QVERIFY(j);

    // try to start the job
    QVERIFY(j->exec());
    QVERIFY(j->status() == KDevelop::VcsJob::JobSucceeded);

    //check if the CVSROOT directory in the new local repository exists now
    QVERIFY(QFileInfo(mercurialRepo).exists());

    //check if isValidDirectory works
    QVERIFY(m_proxy->isValidDirectory(QUrl::fromLocalFile(mercurialTest_BaseDir)));
    //and for non-mercurial dir, I hope nobody has /tmp under mercurial
    QVERIFY(!m_proxy->isValidDirectory(QUrl::fromLocalFile("/tmp")));
}

void MercurialInitTest::addFiles()
{
    mercurialDebug() << "Adding files to the repo";

    //we start it after repoInit, so we still have empty mercurial repo
    {
        QFile f(mercurialTest_BaseDir + mercurialTest_FileName);

        QVERIFY(f.open(QIODevice::WriteOnly));
        QTextStream input(&f);
        input << "HELLO WORLD";
    }
    {
        QFile f(mercurialTest_BaseDir + mercurialTest_FileName2);
        QVERIFY(f.open(QIODevice::WriteOnly));
        QTextStream input(&f);
        input << "No, bar()!";
    }

    //test mercurial-status exitCode (see VcsJob::setExitCode).
    VcsJob *j = m_proxy->status({QUrl::fromLocalFile(mercurialTest_BaseDir)}, KDevelop::IBasicVersionControl::Recursive);
    QVERIFY(j);
    QVERIFY(j->exec());
    QVERIFY(j->status() == KDevelop::VcsJob::JobSucceeded);

    // /tmp/kdevMercurial_testdir/ and kdevMercurial_testdir
    //add always should use aboslute path to the any directory of the repository, let's check:
    j = m_proxy->add({QUrl::fromLocalFile(mercurialTest_BaseDir), QUrl::fromLocalFile(MercurialTestDir1)}, KDevelop::IBasicVersionControl::Recursive);
    QVERIFY(j);
    QVERIFY(j->exec());
    QVERIFY(j->status() == KDevelop::VcsJob::JobSucceeded);

    // /tmp/kdevMercurial_testdir/ and testfile
    j = m_proxy->add({QUrl::fromLocalFile(mercurialTest_BaseDir + mercurialTest_FileName)}, KDevelop::IBasicVersionControl::Recursive);
    QVERIFY(j);
    QVERIFY(j->exec());
    QVERIFY(j->status() == KDevelop::VcsJob::JobSucceeded);

    // TODO: Such ugly code..
    QFile f;
    f.setFileName(mercurialSrcDir + mercurialTest_FileName3);

    if (f.open(QIODevice::WriteOnly)) {
        QTextStream input(&f);
        input << "No, foo()! It's bar()!";
    }
    f.flush();
    f.close();

    //test mercurial-status exitCode again
    j = m_proxy->status({QUrl::fromLocalFile(mercurialTest_BaseDir)}, KDevelop::IBasicVersionControl::Recursive);
    QVERIFY(j);
    QVERIFY(j->exec());
    QVERIFY(j->status() == KDevelop::VcsJob::JobSucceeded);

    //repository path without trailing slash and a file in a parent directory
    // /tmp/repo  and /tmp/repo/src/bar
    j = m_proxy->add({QUrl::fromLocalFile(mercurialSrcDir + mercurialTest_FileName3)});
    QVERIFY(j);
    QVERIFY(j->exec());
    QVERIFY(j->status() == KDevelop::VcsJob::JobSucceeded);

    //let's use absolute path, because it's used in ContextMenus
    j = m_proxy->add({QUrl::fromLocalFile(mercurialTest_BaseDir + mercurialTest_FileName2)}, KDevelop::IBasicVersionControl::Recursive);
    QVERIFY(j);
    QVERIFY(j->exec());
    QVERIFY(j->status() == KDevelop::VcsJob::JobSucceeded);

    //Now let's create several files and try "hg add file1 file2 file3"
    f.setFileName(mercurialTest_BaseDir + "file1");

    if (f.open(QIODevice::WriteOnly)) {
        QTextStream input(&f);
        input << "file1";
    }
    f.flush();
    f.close();

    f.setFileName(mercurialTest_BaseDir + "file2");

    if (f.open(QIODevice::WriteOnly)) {
        QTextStream input(&f);
        input << "file2";
    }
    f.flush();
    f.close();

    j = m_proxy->add({QUrl::fromLocalFile(mercurialTest_BaseDir + "file1"), QUrl::fromLocalFile(mercurialTest_BaseDir + "file2")}, KDevelop::IBasicVersionControl::Recursive);
    QVERIFY(j);
    QVERIFY(j->exec());
    QVERIFY(j->status() == KDevelop::VcsJob::JobSucceeded);
}

void MercurialInitTest::commitFiles()
{
    mercurialDebug() << "Committing...";
    //we start it after addFiles, so we just have to commit
    ///TODO: if "" is ok?
    VcsJob *j = m_proxy->commit(QString("Test commit"), {QUrl::fromLocalFile(mercurialTest_BaseDir)}, KDevelop::IBasicVersionControl::Recursive);
    QVERIFY(j);

    // try to start the job
    QVERIFY(j->exec());
    QVERIFY(j->status() == KDevelop::VcsJob::JobSucceeded);

    //test mercurial-status exitCode one more time.
    j = m_proxy->status({QUrl::fromLocalFile(mercurialTest_BaseDir)}, KDevelop::IBasicVersionControl::Recursive);
    QVERIFY(j);

    QVERIFY(j->exec());
    QVERIFY(j->status() == KDevelop::VcsJob::JobSucceeded);

    //Test the results of the "mercurial add"
    DVcsJob *jobLs = new DVcsJob(mercurialTest_BaseDir, 0);
    *jobLs << "hg" << "stat" << "-q" << "-c" << "-n";
    QVERIFY(jobLs->exec());
    QVERIFY(jobLs->status() == KDevelop::VcsJob::JobSucceeded);

    QStringList files = jobLs->output().split("\n");
    QVERIFY(files.contains(mercurialTest_FileName));
    QVERIFY(files.contains(mercurialTest_FileName2));
    QVERIFY(files.contains("src/" + mercurialTest_FileName3));

    mercurialDebug() << "Committing one more time";

    //let's try to change the file and test "hg commit -a"
    QFile f(mercurialTest_BaseDir + mercurialTest_FileName);

    if (f.open(QIODevice::WriteOnly)) {
        QTextStream input(&f);
        input << "Just another HELLO WORLD";
    }
    f.flush();
    f.close();

    //add changes
    j = m_proxy->add({QUrl::fromLocalFile(mercurialTest_BaseDir + mercurialTest_FileName)}, KDevelop::IBasicVersionControl::Recursive);
    QVERIFY(j);

    // try to start the job
    QVERIFY(j->exec());
    QVERIFY(j->status() == KDevelop::VcsJob::JobSucceeded);

    j = m_proxy->commit(QString("KDevelop's Test commit2"), {QUrl::fromLocalFile(mercurialTest_BaseDir)}, KDevelop::IBasicVersionControl::Recursive);
    QVERIFY(j);

    // try to start the job
    QVERIFY(j->exec());
    QVERIFY(j->status() == KDevelop::VcsJob::JobSucceeded);
}

void MercurialInitTest::cloneRepository()
{
    // make job that clones the local repository, created in the previous test
    VcsJob *j = m_proxy->createWorkingCopy(VcsLocation(mercurialTest_BaseDir), QUrl::fromLocalFile(mercurialTest_BaseDir2));
    QVERIFY(j);

    // try to start the job
    QVERIFY(j->exec());

    //check if the .hg directory in the new local repository exists now
    QVERIFY(QFileInfo(QString(mercurialTest_BaseDir2 + "/.hg/")).exists());
}

void MercurialInitTest::testInit()
{
    repoInit();
}

void MercurialInitTest::testAdd()
{
    addFiles();
}

void MercurialInitTest::testCommit()
{
    commitFiles();
}

void MercurialInitTest::testBranching()
{
}

void MercurialInitTest::revHistory()
{
    QList<DVcsEvent> commits = m_proxy->getAllCommits(mercurialTest_BaseDir);
    QCOMPARE(commits.count(), 2);
    QCOMPARE(commits[0].getParents().size(), 1); //initial commit is on the top
    QVERIFY(commits[1].getParents().isEmpty());  //0 is later than 1!
    QCOMPARE(commits[0].getLog(), QString("KDevelop's Test commit2"));  //0 is later than 1!
    QCOMPARE(commits[1].getLog(), QString("Test commit"));
    QVERIFY(commits[1].getCommit().contains(QRegExp("^\\w{,40}$")));
    QVERIFY(commits[0].getCommit().contains(QRegExp("^\\w{,40}$")));
    QVERIFY(commits[0].getParents()[0].contains(QRegExp("^\\w{,40}$")));
}

void MercurialInitTest::removeTempDirs()
{
    if (QFileInfo(mercurialTest_BaseDir).exists())
        if (!(KIO::del(QUrl::fromLocalFile(mercurialTest_BaseDir))->exec()))
            mercurialDebug() << "KIO::del(" << mercurialTest_BaseDir << ") returned false";

    if (QFileInfo(mercurialTest_BaseDir2).exists())
        if (!(KIO::del(QUrl::fromLocalFile(mercurialTest_BaseDir2))->exec()))
            mercurialDebug() << "KIO::del(" << mercurialTest_BaseDir2 << ") returned false";
}

QTEST_MAIN(MercurialInitTest)

// #include "mercurialtest.moc"
