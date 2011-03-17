/* ============================================================
* QupZilla - WebKit based browser
* Copyright (C) 2010-2011  nowrep
*
* This program is free software: you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation, either version 3 of the License, or
* (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program.  If not, see <http://www.gnu.org/licenses/>.
* ============================================================ */
#include "rssmanager.h"
#include "ui_rssmanager.h"
#include "qupzilla.h"
#include "locationbar.h"
#include "tabwidget.h"
#include "mainapplication.h"
#include "treewidget.h"

RSSManager::RSSManager(QupZilla* mainClass, QWidget* parent) :
    QWidget(parent)
    ,ui(new Ui::RSSManager)
    ,p_QupZilla(mainClass)
{
    ui->setupUi(this);
//    CENTER on scren
    const QRect screen = QApplication::desktop()->screenGeometry();
    const QRect &size = geometry();
    QWidget::move( (screen.width()-size.width())/2, (screen.height()-size.height())/2 );

    ui->tabWidget->setElideMode(Qt::ElideRight);
    m_networkManager = new QNetworkAccessManager();
    connect(ui->buttonBox, SIGNAL(clicked(QAbstractButton*)), this, SLOT(close()));
    connect(ui->reload, SIGNAL(clicked()), this, SLOT(reloadFeed()));
    connect(ui->deletebutton, SIGNAL(clicked()), this, SLOT(deleteFeed()));
    connect(ui->edit, SIGNAL(clicked()), this, SLOT(editFeed()));
}

QupZilla* RSSManager::getQupZilla()
{
    if (!p_QupZilla)
        p_QupZilla = mApp->getWindow();
    return p_QupZilla;
}

void RSSManager::setMainWindow(QupZilla* window)
{
    if (window)
        p_QupZilla = window;
}

void RSSManager::refreshTable()
{
    QSqlQuery query;
    query.exec("SELECT count(id) FROM rss");
    if (!query.next())
        return;
    if (query.value(0).toInt() == 0)
        return;

    ui->tabWidget->clear();
    query.exec("SELECT address, title FROM rss");
    int i = 0;
    while (query.next()) {
        QUrl address = query.value(0).toUrl();
        QString title = query.value(1).toString();
        TreeWidget* tree = new TreeWidget();
        tree->setHeaderLabel(tr("News"));
        tree->setContextMenuPolicy(Qt::CustomContextMenu);
        connect(tree, SIGNAL(customContextMenuRequested(const QPoint &)), this, SLOT(customContextMenuRequested(const QPoint &)));

        ui->tabWidget->addTab(tree, title);
        ui->tabWidget->setTabToolTip(i, address.toString());
        connect(tree, SIGNAL(itemDoubleClicked(QTreeWidgetItem*,int)), this, SLOT(loadFeed(QTreeWidgetItem*)));
        connect(tree, SIGNAL(itemControlClicked(QTreeWidgetItem*)), this, SLOT(controlLoadFeed(QTreeWidgetItem*)));
        QTreeWidgetItem* item = new QTreeWidgetItem();
        item->setText(0, tr("Loading..."));
        tree->addTopLevelItem(item);

        QIcon icon = LocationBar::icon(address);

        if (icon.pixmap(16,16).toImage() == QIcon(":/icons/locationbar/unknownpage.png").pixmap(16,16).toImage())
            icon = QIcon(":/icons/menu/rss.png");
        ui->tabWidget->setTabIcon(i, icon );
        beginToLoadSlot(address);
        i++;
    }
    if (i > 0) {
        ui->deletebutton->setEnabled(true);
        ui->reload->setEnabled(true);
        ui->edit->setEnabled(true);
    }
}

void RSSManager::reloadFeed()
{
    TreeWidget* treeWidget = qobject_cast<TreeWidget*>(ui->tabWidget->widget(ui->tabWidget->currentIndex()));
    if (!treeWidget)
        return;
    treeWidget->clear();
    QTreeWidgetItem* item = new QTreeWidgetItem();
    item->setText(0, tr("Loading..."));
    treeWidget->addTopLevelItem(item);

    beginToLoadSlot( QUrl(ui->tabWidget->tabToolTip(ui->tabWidget->currentIndex())) );
}

void RSSManager::deleteFeed()
{
    QString url = ui->tabWidget->tabToolTip(ui->tabWidget->currentIndex());
    if (url.isEmpty())
        return;
    QSqlQuery query;
    query.exec("DELETE FROM rss WHERE address='"+url+"'");

    ui->tabWidget->removeTab(ui->tabWidget->currentIndex());
    if (ui->tabWidget->count() == 0) {
        ui->deletebutton->setEnabled(false);
        ui->reload->setEnabled(false);
        ui->edit->setEnabled(false);
        refreshTable();
    }
}

void RSSManager::editFeed()
{
    QString url = ui->tabWidget->tabToolTip(ui->tabWidget->currentIndex());
    if (url.isEmpty())
        return;

    QDialog* dialog = new QDialog(this);
    QFormLayout* layout = new QFormLayout(dialog);
    QLabel* label = new QLabel(dialog);
    QLineEdit* editUrl = new QLineEdit(dialog);
    QLineEdit* editTitle = new QLineEdit(dialog);
    QDialogButtonBox* box = new QDialogButtonBox(dialog);
    box->addButton(QDialogButtonBox::Ok);
    box->addButton(QDialogButtonBox::Cancel);
    connect(box, SIGNAL(rejected()), dialog, SLOT(reject()));
    connect(box, SIGNAL(accepted()), dialog, SLOT(accept()));

    label->setText(tr("Fill title and URL of a feed: "));
    layout->addRow(label);
    layout->addRow(new QLabel(tr("Feed title: ")), editTitle);
    layout->addRow(new QLabel(tr("Feed URL: ")), editUrl);
    layout->addRow(box);

    editUrl->setText( ui->tabWidget->tabToolTip(ui->tabWidget->currentIndex()) );
    editTitle->setText( ui->tabWidget->tabText(ui->tabWidget->currentIndex()) );

    dialog->setWindowTitle(tr("Edit RSS Feed"));
    dialog->setMinimumSize(400, 100);
    dialog->exec();
    if (dialog->result() == QDialog::Rejected)
        return;

    QString address = editUrl->text();
    QString title = editTitle->text();

    if (address.isEmpty() || title.isEmpty())
        return;

    QSqlQuery query;
    query.prepare("UPDATE rss SET address=?, title=? WHERE address=?");
    query.bindValue(0, address);
    query.bindValue(1, title);
    query.bindValue(2, url);
    query.exec();

    refreshTable();

}

void RSSManager::customContextMenuRequested(const QPoint &position)
{
    TreeWidget* treeWidget = qobject_cast<TreeWidget*>(ui->tabWidget->widget(ui->tabWidget->currentIndex()));
    if (!treeWidget)
        return;

    if (!treeWidget->itemAt(position))
        return;

    QString link = treeWidget->itemAt(position)->toolTip(0);
    if (link.isEmpty())
        return;

    QMenu menu;
    menu.addAction(tr("Open link in actual tab"), getQupZilla(), SLOT(loadActionUrl()))->setData(link);
    menu.addAction(tr("Open link in new tab"), this, SLOT(loadFeedInNewTab()))->setData(link);
    menu.addSeparator();
    menu.addAction(tr("Close"), this, SLOT(close()));

    //Prevent choosing first option with double rightclick
    QPoint pos = QCursor::pos();
    QPoint p(pos.x(), pos.y()+1);
    menu.exec(p);
}

void RSSManager::loadFeed(QTreeWidgetItem* item)
{
    if (!item)
        return;
    if (item->whatsThis(0).isEmpty())
        return;
    getQupZilla()->loadAddress(QUrl(item->whatsThis(0)));
}

void RSSManager::controlLoadFeed(QTreeWidgetItem* item)
{
    if (!item)
        return;
    if (item->whatsThis(0).isEmpty())
        return;
    getQupZilla()->tabWidget()->addView(QUrl(item->whatsThis(0)), tr("New Tab"), TabWidget::NewNotSelectedTab);
}

void RSSManager::loadFeedInNewTab()
{
    if (QAction* action = qobject_cast<QAction*>(sender()))
        getQupZilla()->tabWidget()->addView(action->data().toUrl(), tr("New Tab"), TabWidget::NewNotSelectedTab);
}

void RSSManager::beginToLoadSlot(const QUrl &url)
{
    QNetworkReply* reply;
    reply=m_networkManager->get(QNetworkRequest(QUrl(url)));

    connect(m_networkManager, SIGNAL(finished(QNetworkReply*)), this, SLOT(finished(QNetworkReply*)));
}

void RSSManager::finished(QNetworkReply* reply)
{
    if (m_networkReplies.contains(reply))
        return;

    QString currentTag;
    QString linkString;
    QString titleString;
    QXmlStreamReader xml;
    xml.addData(reply->readAll());

    int tabIndex = -1;
    for (int i=0; i<ui->tabWidget->count(); i++) {
        QString replyUrl = reply->url().toString();
        if (replyUrl == ui->tabWidget->tabToolTip(i)) {
            tabIndex = i;
            break;
        }
    }

    if (tabIndex == -1)
        return;

    TreeWidget* treeWidget = qobject_cast<TreeWidget*>(ui->tabWidget->widget(tabIndex));
    if (!treeWidget)
        return;
    treeWidget->clear();

    while (!xml.atEnd()) {
        xml.readNext();
        if (xml.isStartElement()) {
            if (xml.name() == "item")
                linkString = xml.attributes().value("rss:about").toString();
            currentTag = xml.name().toString();
            } else if (xml.isEndElement()) {
                if (xml.name() == "item") {
                    QTreeWidgetItem* item = new QTreeWidgetItem;
                    item->setText(0, titleString);
                    item->setWhatsThis(0, linkString);
                    item->setIcon(0, QIcon(":/icons/other/feed.png"));
                    item->setToolTip(0, linkString);
                    treeWidget->addTopLevelItem(item);

                    titleString.clear();
                    linkString.clear();
                }
            } else if (xml.isCharacters() && !xml.isWhitespace()) {
                if (currentTag == "title")
                    titleString = xml.text().toString();
                else if (currentTag == "link")
                    linkString += xml.text().toString();
            }
    }

    if (treeWidget->topLevelItemCount() == 0) {
        QTreeWidgetItem* item = new QTreeWidgetItem;
        item->setText(0, tr("Error in fetching feed"));
        treeWidget->addTopLevelItem(item);
    }

    m_networkReplies.append(reply);
}

bool RSSManager::addRssFeed(const QString &address, const QString &title)
{
    if (address.isEmpty())
        return false;
    QSqlQuery query;
    query.exec("SELECT id FROM rss WHERE address='"+address+"'");
    if (!query.next()) {
        query.prepare("INSERT INTO rss (address, title) VALUES(?,?)");
        query.bindValue(0, address);
        query.bindValue(1, title);
        query.exec();
        return true;
    }

    QMessageBox::warning(getQupZilla(), tr("RSS feed duplicated"), tr("You already have this feed."));
    return false;
}

RSSManager::~RSSManager()
{
    delete ui;
}
