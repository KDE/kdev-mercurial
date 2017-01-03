/*
    This file is part of KDevelop

    Copyright 2008 Evgeniy Ivanov <powerfox@kde.ru>
    Copyright 2009 Fabian Wiesel <fabian.wiesel@fu-berlin.de>
    Copyright 2017 Sergey Kalinichev <kalinichev.so.0@gmail.com>

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/

#include "initTest.h"

#include <QtTest/QtTest>
#include <tests/testcore.h>
#include <tests/autotestshell.h>

#include <QUrl>

#include <KIO/DeleteJob>

#include <vcs/dvcs/dvcsjob.h>
#include "../mercurialplugin.h"
#include "debug.h"

using namespace KDevelop;

namespace
{
const QString tempDir = QDir::tempPath();
const QString MercurialTestDir1("kdevMercurial_testdir");
const QString mercurialTest_BaseDir(tempDir + "/kdevMercurial_testdir/");
const QString mercurialTest_BaseDir2(tempDir + "/kdevMercurial_testdir2/");
const QString mercurialRepo(mercurialTest_BaseDir + ".hg");
const QString mercurialSrcDir(mercurialTest_BaseDir + "src/");
const QString mercurialTest_FileName("testfile");
const QString mercurialTest_FileName2("foo");
const QString mercurialTest_FileName3("bar");

void verifyJobSucceed(VcsJob* job)
{
    QVERIFY(job);
    QVERIFY(job->exec());
    QVERIFY(job->status() == KDevelop::VcsJob::JobSucceeded);
}

void writeToFile(const QString& path, const QString& content)
{
    QFile f(path);
    QVERIFY(f.open(QIODevice::WriteOnly));
    QTextStream input(&f);
    input << content;
}

}

void MercurialInitTest::initTestCase()
{
    AutoTestShell::init({"kdevmercurial"});
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

    removeTempDirs();
}

void MercurialInitTest::repoInit()
{
    mercurialDebug() << "Trying to init repo";
    // create the local repository
    VcsJob *j = m_proxy->init(QUrl::fromLocalFile(mercurialTest_BaseDir));
    verifyJobSucceed(j);
    QVERIFY(QFileInfo(mercurialRepo).exists());

    QVERIFY(m_proxy->isValidDirectory(QUrl::fromLocalFile(mercurialTest_BaseDir)));
    QVERIFY(!m_proxy->isValidDirectory(QUrl::fromLocalFile("/tmp")));
}

void MercurialInitTest::addFiles()
{
    mercurialDebug() << "Adding files to the repo";

    // we start it after repoInit, so we still have empty mercurial repo
    writeToFile(mercurialTest_BaseDir + mercurialTest_FileName, "HELLO WORLD");
    writeToFile(mercurialTest_BaseDir + mercurialTest_FileName2, "No, bar()!");

    // TODO: check status output
    VcsJob *j = m_proxy->status({QUrl::fromLocalFile(mercurialTest_BaseDir)}, KDevelop::IBasicVersionControl::Recursive);
    verifyJobSucceed(j);

    // /tmp/kdevMercurial_testdir/ and kdevMercurial_testdir
    //add always should use aboslute path to the any directory of the repository, let's check:
    j = m_proxy->add({QUrl::fromLocalFile(mercurialTest_BaseDir), QUrl::fromLocalFile(MercurialTestDir1)}, KDevelop::IBasicVersionControl::Recursive);
    verifyJobSucceed(j);

    // /tmp/kdevMercurial_testdir/ and testfile
    j = m_proxy->add({QUrl::fromLocalFile(mercurialTest_BaseDir + mercurialTest_FileName)}, KDevelop::IBasicVersionControl::Recursive);
    verifyJobSucceed(j);

    writeToFile(mercurialSrcDir + mercurialTest_FileName3, "No, foo()! It's bar()!");

    // TODO: check status output
    j = m_proxy->status({QUrl::fromLocalFile(mercurialTest_BaseDir)}, KDevelop::IBasicVersionControl::Recursive);
    verifyJobSucceed(j);

    // repository path without trailing slash and a file in a parent directory
    // /tmp/repo  and /tmp/repo/src/bar
    j = m_proxy->add({QUrl::fromLocalFile(mercurialSrcDir + mercurialTest_FileName3)});
    verifyJobSucceed(j);

    // let's use absolute path, because it's used in ContextMenus
    j = m_proxy->add({QUrl::fromLocalFile(mercurialTest_BaseDir + mercurialTest_FileName2)}, KDevelop::IBasicVersionControl::Recursive);
    verifyJobSucceed(j);

    //Now let's create several files and try "hg add file1 file2 file3"
    writeToFile(mercurialTest_BaseDir + "file1", "file1");
    writeToFile(mercurialTest_BaseDir + "file2", "file2");

    j = m_proxy->add({QUrl::fromLocalFile(mercurialTest_BaseDir + "file1"), QUrl::fromLocalFile(mercurialTest_BaseDir + "file2")}, KDevelop::IBasicVersionControl::Recursive);
    verifyJobSucceed(j);
}

void MercurialInitTest::commitFiles()
{
    mercurialDebug() << "Committing...";
    // we start it after addFiles, so we just have to commit
    ///TODO: if "" is ok?
    VcsJob *j = m_proxy->commit(QString("Test commit"), {QUrl::fromLocalFile(mercurialTest_BaseDir)}, KDevelop::IBasicVersionControl::Recursive);
    verifyJobSucceed(j);

    j = m_proxy->status({QUrl::fromLocalFile(mercurialTest_BaseDir)}, KDevelop::IBasicVersionControl::Recursive);
    verifyJobSucceed(j);

    // Test the results of the "mercurial add"
    DVcsJob *jobLs = new DVcsJob(mercurialTest_BaseDir, nullptr);
    *jobLs << "hg" << "stat" << "-q" << "-c" << "-n";
    verifyJobSucceed(jobLs);

    QStringList files = jobLs->output().split("\n");
    QVERIFY(files.contains(mercurialTest_FileName));
    QVERIFY(files.contains(mercurialTest_FileName2));
    QVERIFY(files.contains("src/" + mercurialTest_FileName3));

    mercurialDebug() << "Committing one more time";

    // let's try to change the file and test "hg commit -a"
    writeToFile(mercurialTest_BaseDir + mercurialTest_FileName, "Just another HELLO WORLD");

    j = m_proxy->add({QUrl::fromLocalFile(mercurialTest_BaseDir + mercurialTest_FileName)}, KDevelop::IBasicVersionControl::Recursive);
    verifyJobSucceed(j);

    j = m_proxy->commit(QString("KDevelop's Test commit2"), {QUrl::fromLocalFile(mercurialTest_BaseDir)}, KDevelop::IBasicVersionControl::Recursive);
    verifyJobSucceed(j);
}

void MercurialInitTest::cloneRepository()
{
    // make job that clones the local repository, created in the previous test
    VcsJob *j = m_proxy->createWorkingCopy(VcsLocation(mercurialTest_BaseDir), QUrl::fromLocalFile(mercurialTest_BaseDir2));
    verifyJobSucceed(j);
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
