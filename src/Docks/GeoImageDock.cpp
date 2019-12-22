#include "GeoImageDock.h"

#include "Node.h"
#include "Layer.h"
#include "DocumentCommands.h"
#include "LayerWidget.h"
#include "PropertiesDock.h"
#include "Global.h"

#ifdef USE_ZBAR
#include <zbar.h>
#include <zbar/QZBarImage.h>
#endif

#include <QInputDialog>
#include <QMessageBox>
#include <QProgressDialog>
#include <QClipboard>
#include <QRadioButton>
#include <QTimeEdit>
#include <QDialogButtonBox>
#include <QFileDialog>

#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>

#include <algorithm>
#include <memory>

#include "ui_PhotoLoadErrorDialog.h"

#define WARNING(title, message) { \
    if (QMessageBox::warning(this, title, message.arg(file), \
     QMessageBox::Ignore | QMessageBox::Cancel, QMessageBox::Ignore) == QMessageBox::Ignore) \
        continue; \
    else { \
        theView->invalidate(true, true, false); \
        return; \
    } \
}

#define QUESTION(title, message, always) { \
    if (always == 0) { \
        int replyButton = QMessageBox::question(this, title, message, \
         QMessageBox::Yes | QMessageBox::YesToAll | QMessageBox::No | QMessageBox::NoToAll | QMessageBox::Abort, QMessageBox::Yes ); \
        if (replyButton == QMessageBox::No) \
            continue; \
        else if (replyButton == QMessageBox::Abort) { \
            theView->invalidate(true, true, false); \
            return; \
        } \
        else if (replyButton != QMessageBox::Yes) \
            always = replyButton; \
    } \
    if (always == QMessageBox::NoToAll) \
        continue; \
}

bool GeoImageDock::getWalkingPapersDetails(const QUrl& reqUrl, double &lat, double &lon, bool& positionValid) const
{
    QNetworkAccessManager manager;
    QEventLoop q;
    QTimer tT;

    if (!reqUrl.host().contains("walking-papers.org"))
        return false;

    tT.setSingleShot(true);
    connect(&tT, SIGNAL(timeout()), &q, SLOT(quit()));
    connect(&manager, SIGNAL(finished(QNetworkReply*)),
            &q, SLOT(quit()));
    QNetworkReply *reply = manager.get(QNetworkRequest(reqUrl));

    tT.start(M_PREFS->getNetworkTimeout());
    q.exec();
    if(tT.isActive()) {
        // download complete
        tT.stop();
    } else {
        QMessageBox::warning(0, tr("Network timeout"), tr("Cannot read the photo's details from the Walking Papers server."), QMessageBox::Ok);
        return false;
    }

    QString center = QString::fromLatin1(reply->rawHeader("X-Print-Center"));
    QStringList sl = center.split(" ");
    if (sl.size() != 3)
        return false;

    //int z = sl[2].toInt();
    lat = sl[0].toDouble();
    lon = sl[1].toDouble();
    positionValid = true;

    return true;
}

bool GeoImageDock::askAndgetWalkingPapersDetails(double &lat, double &lon, bool& positionValid) const
{
    bool ok;
    QString text = QInputDialog::getText(0, tr("Please specify Walking Papers URL"),
                                         tr("URL:"), QLineEdit::Normal, "", &ok);
    if (ok && !text.isEmpty()) {
        QUrl url(text);
        return getWalkingPapersDetails(url, lat, lon, positionValid);
    } else
        return false;
}

GeoImageDock::GeoImageDock(MainWindow *aMain)
    : MDockAncestor(aMain), Main(aMain)
    , photoLayer(0)
{
    curImage = lastImage = -1;
    updateByMe = false;
    setWindowTitle(tr("Geo Images"));
    Image = new ImageView(this);
    setWidget(Image);
    setObjectName("geoImageDock");

    setFocusPolicy(Qt::StrongFocus);
    setContextMenuPolicy(Qt::ActionsContextMenu);

    centerAction = new QAction(tr("Center map"), this);
    centerAction->setShortcut(tr("c"));
    remImagesAction = new QAction(tr("Remove Images"), this);
    toClipboardAction = new QAction(tr("Copy filename to clipboard"), this);
    nextImageAction = new QAction(tr("Select next image"), this);
    nextImageAction->setShortcut(tr("PgDown"));
    nextImageAction->setShortcutContext(Qt::WidgetWithChildrenShortcut);
    previousImageAction = new QAction(tr("Select previous image"), this);
    previousImageAction->setShortcut(tr("PgUp"));
    previousImageAction->setShortcutContext(Qt::WidgetWithChildrenShortcut);
    saveImageAction = new QAction(tr("Save geotagged image..."), this);

    QAction* sep = new QAction(this);
    sep->setSeparator(true);

    addAction(centerAction);
    addAction(nextImageAction);
    addAction(previousImageAction);
    addAction(toClipboardAction);
    addAction(saveImageAction);
    addAction(sep);
    addAction(remImagesAction);

    connect(centerAction, SIGNAL(triggered()), this, SLOT(centerMap()));
    connect(remImagesAction, SIGNAL(triggered()), this, SLOT(removeImages()));
    connect(toClipboardAction, SIGNAL(triggered()), this, SLOT(toClipboard()));
    connect(nextImageAction, SIGNAL(triggered()), this, SLOT(selectNext()));
    connect(previousImageAction, SIGNAL(triggered()), this, SLOT(selectPrevious()));
    connect(saveImageAction, SIGNAL(triggered()), this, SLOT(saveImage()));
}

GeoImageDock::~GeoImageDock(void)
{
    delete widget();
}

void GeoImageDock::clear()
{
    usedTrackPoints.clear();
    curImage = lastImage = -1;
    Image->setImage("");
}

void GeoImageDock::setImage(Node *Pt)
{
    if (curImage != -1) // save last imageId to start iteration there again
        lastImage = curImage;
    if (updateByMe)
        return;
    if (!Pt) {
        Image->setImage("");
        curImage = -1;
        return;
    }

    int ImageId;
    for (ImageId = 0; ImageId < usedTrackPoints.size(); ImageId++) // search for an entry in our list
        if (usedTrackPoints.at(ImageId).node == Pt)
            break;

    if (ImageId == curImage)
        return;

    if (ImageId == usedTrackPoints.size()) { // haven't found it
        Image->setImage("");
        curImage = -1;
        return;
    }

    Image->setImage(usedTrackPoints.at(ImageId).filename, (Pt ? true : false));
    curImage = ImageId;
}

void GeoImageDock::setImage(int ImageId)
{
    if (curImage != -1) // save last imageid to start iteration there again
        lastImage = curImage;
    if (ImageId < 0 || ImageId >= usedTrackPoints.size()) { // invalid ImageId
        Image->setImage("");
        curImage = -1;
        return;
    }

    int lookImage = ImageId;

    if (usedTrackPoints.at(lookImage).node) {
        bool ok = false;
        Feature* theFeature;
        while (!ok) {
            FeatureIterator it(Main->document());
            for (; !it.isEnd(); ++it) // find TrackPoint
                if (usedTrackPoints.at(lookImage).node == it.get()) {
                    break;
                }
            if (it.isEnd())
                usedTrackPoints.removeAt(ImageId);
            if (it.isEnd() || !it.get()->isVisible()) {
                if (usedTrackPoints.size()) {
                    if (++lookImage >= usedTrackPoints.size())
                        lookImage = 0;
                    if (lookImage == ImageId) {
                        Image->setImage("");
                        curImage = -1;
                        return;
                    }
                } else
                    break;
            } else {
                theFeature = it.get();
                ok = true;
            }
        }

//        if (!ok) { // haven't found one
//            Image->setImage("");
//            curImage = -1;
//            return;
//        }

        updateByMe = true;
        if (!Main->properties()->isSelected(theFeature)) {
            Main->properties()->setSelection(theFeature);
            Main->view()->invalidate(true, true, false);
        }
        updateByMe = false;
    }

    Image->setImage(usedTrackPoints.at(lookImage).filename, (usedTrackPoints.at(lookImage).node ? true : false));
    curImage = lookImage;
}

void GeoImageDock::removeImages(void)
{
    int i;

    for (i = 0; i < usedTrackPoints.size(); i++) {
        Node *Pt = usedTrackPoints.at(i).node;
        if (!Pt) {
            continue;
        }
        if (usedTrackPoints.at(i).inserted) {
            Pt->layer()->remove(Pt);
            g_backend.deallocFeature(Pt->layer(), Pt);
        }
        else
            Pt->clearTag("Picture");
    }

    usedTrackPoints.clear();
    curImage = -1;
    Image->setImage("");

    Main->view()->invalidate(true, true, false);
}

void GeoImageDock::toClipboard(void)
{
    if (curImage != -1) {
        QClipboard *clipboard = QApplication::clipboard();

        clipboard->setText(usedTrackPoints.at(curImage).filename);
    }
}

void GeoImageDock::selectNext(void)
{
    if (curImage == -1) // restart iteration at last selected image
        curImage = lastImage;
    if (++curImage >= usedTrackPoints.size())
        curImage = 0;

    setImage(curImage);
}
void GeoImageDock::selectPrevious(void)
{
    if (curImage == -1) // restart iteration at last selected image
        curImage = lastImage;
    if (--curImage < 0)
        curImage = usedTrackPoints.size() - 1;

    setImage(curImage);
}

void GeoImageDock::centerMap(void)
{
    int index = curImage;
    if (index == -1)
        index = lastImage;
    if (index < 0 || index >= usedTrackPoints.size()) { // invalid ImageId
        return;
    }
    Feature* f = usedTrackPoints.at(index).node;
    if (f && !f->isNull()) {
        Coord c = f->boundingBox().center();
        Main->view()->setCenter(c, Main->view()->rect());
        Main->invalidateView();
    }
}

void GeoImageDock::addUsedTrackpoint(NodeData data)
{
    for(int i=0; i<usedTrackPoints.size(); ++i) {
        if (usedTrackPoints[i].filename == data.filename) {
            usedTrackPoints[i] = data;
            return;
        }
    }

    usedTrackPoints << data;
}

void GeoImageDock::loadImage(QString file, Coord pos)
{
    Document *theDocument = Main->document();
    //MapView *theView = Main->view();

    Layer *theLayer;
    if (photoLayer == nullptr) {
        photoLayer = new TrackLayer(tr("Photo layer"));
        photoLayer->setReadonly(false);
        theDocument->add(photoLayer);
    }

    { // retrieve the target layer from the user
        QStringList layers;
        QList<int> layerId;
        int i;
        Layer *layer;
        Layer *singleLayer = nullptr;
        Layer *singleTrackLayer = nullptr;
        int trackLayersCount = 0;
        for (i=0;i<theDocument->layerSize();i++) {
            layer = theDocument->getLayer(i);
            if (!layer->isEnabled())
                continue;
            if (layer->classType() == Layer::TrackLayerType) {
                trackLayersCount++;
                if (!singleTrackLayer)
                    singleTrackLayer = layer;
            }
            if (layer->classType() == Layer::TrackLayerType || layer->classType() == Layer::DrawingLayerType) {
                if (!singleLayer)
                    singleLayer = layer;
                layers.append(theDocument->getLayer(i)->name());
                layerId.append(i);
            }
        }

        // Select single layer if there is only one
        if (layers.size() == 1)
        {
            theLayer = singleLayer;
        }
        // Select single track layer if there is only one
        else if (trackLayersCount == 1)
        {
            theLayer = singleTrackLayer;
        }
        // Now ask the user what layer to add the photos to
        else
        {
            bool ok;
            QString name = QInputDialog::getItem(nullptr, tr("Load geotagged Images"),
             tr("Select the layer to which the images belong:"), layers, 0, false, &ok);
            if (ok && !name.isEmpty())
                theLayer = theDocument->getLayer(layerId.at(layers.indexOf(name)));
            else
                return;
        }
        if (theLayer != photoLayer && !photoLayer->size()) {
            theDocument->remove(photoLayer);
            SAFE_DELETE(photoLayer)
        }
    }

    if (theLayer->isReadonly()) { // nodes from readonly layers can not be selected and therefore associated images can not be displayed
        if (QMessageBox::question(this, tr("Layer is read-only"),
         tr("The used layer is not writeable. Should it be made writeable?\nIf not, you can't load images that belong to it."),
         QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Yes) == QMessageBox::Yes)
            theLayer->getWidget()->setLayerReadonly(false); // this makes/updates both the widget and the layer with readonly = false
        else
            return;
    }

    Node *Pt = nullptr;
    int i = 0;
    for (; i<theLayer->size(); ++i) // use existing TrackPoint if there is one in small distance
        if ((Pt = CAST_NODE(theLayer->get(i))) &&
         Pt->position().distanceFrom(pos) <= .002)
            break;
        else
            Pt = nullptr;

    PhotoNode* phNode;
    if (!Pt) {
        phNode = g_backend.allocPhotoNode(theLayer, pos);
        theLayer->add(Pt);
        g_backend.sync(Pt);
    } else {
        if (CAST_TRACKNODE(Pt))
            phNode = g_backend.allocPhotoNode(theLayer, *CAST_TRACKNODE(Pt));
        else
            phNode = g_backend.allocPhotoNode(theLayer, *Pt);
        theLayer->add(phNode);
        for (int i=0; i<Pt->sizeParents(); ++i) {
            Feature *P = CAST_FEATURE(Pt->getParent(i));
            int idx = P->find(Pt);
            if (Way* W = CAST_WAY(P)) {
                W->add(phNode, idx);
            } else if (Relation* R = CAST_RELATION(P)) {
                R->add(R->getRole(idx), phNode, idx);
            } else if (TrackSegment* S = CAST_SEGMENT(P)) {
                S->add(phNode, idx);
            }
            P->remove(idx+1);
            g_backend.deallocFeature(theLayer, Pt);

        }

    }

    QDateTime time = QFileInfo(file).created();

    //Pt->setTag("_waypoint_", "true");
    phNode->setTag("_picture_", "GeoTagged");
    phNode->setPhoto(QPixmap(file));
    addUsedTrackpoint(NodeData(phNode, file, time, i == theLayer->size()));
}

void GeoImageDock::loadImages(QStringList fileNames)
{
    QString file;
    QDateTime time;
    int offset = -1, noMatchQuestion = 0;

    Document *theDocument = Main->document();
    MapView *theView = Main->view();

    Exiv2::Image::AutoPtr image;
    Exiv2::ExifData exifData;
    bool positionValid = false;

    Layer *theLayer;
    if (photoLayer == nullptr) {
        photoLayer = new TrackLayer(tr("Photo layer"));
        photoLayer->setReadonly(false);
        theDocument->add(photoLayer);
    }

    { // retrieve the target layer from the user
        QStringList layers;
        QList<int> layerId;
        int i;
        Layer *layer;
        Layer *singleLayer = nullptr;
        Layer *singleTrackLayer = nullptr;
        int trackLayersCount = 0;
        for (i=0;i<theDocument->layerSize();i++) {
            layer = theDocument->getLayer(i);
            if (!layer->isEnabled())
                continue;
            if (layer->classType() == Layer::TrackLayerType) {
                trackLayersCount++;
                if (!singleTrackLayer)
                    singleTrackLayer = layer;
            }
            if (layer->classType() == Layer::TrackLayerType || layer->classType() == Layer::DrawingLayerType) {
                if (!singleLayer)
                    singleLayer = layer;
                layers.append(theDocument->getLayer(i)->name());
                layerId.append(i);
            }
        }

        // Select single layer if there is only one
        if (layers.size() == 1)
        {
            theLayer = singleLayer;
        }
        // Select single track layer if there is only one
        else if (trackLayersCount == 1)
        {
            theLayer = singleTrackLayer;
        }
        // Now ask the user what layer to add the photos to
        else
        {
            bool ok;
            QString name = QInputDialog::getItem(nullptr, tr("Load geotagged Images"),
             tr("Select the layer to which the images belong:"), layers, 0, false, &ok);
            if (ok && !name.isEmpty())
                theLayer = theDocument->getLayer(layerId.at(layers.indexOf(name)));
            else
                return;
        }
        if (theLayer != photoLayer && !photoLayer->size()) {
            theDocument->remove(photoLayer);
            SAFE_DELETE(photoLayer);
        }
    }

    if (theLayer->isReadonly()) { // nodes from readonly layers can not be selected and therefore associated images can not be displayed
        if (QMessageBox::question(this, tr("Layer is read-only"),
         tr("The used layer is not writeable. Should it be made writeable?\nIf not, you can't load images that belong to it."),
         QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Yes) == QMessageBox::Yes)
            theLayer->getWidget()->setLayerReadonly(false); // this makes/updates both the widget and the layer with readonly = false
        else
            return;
    }

    QProgressDialog progress(tr("Loading Images ..."), tr("Abort loading"), 0, fileNames.size());
    progress.setWindowFlags(progress.windowFlags() & ~Qt::WindowContextHelpButtonHint);
    progress.setWindowFlags(progress.windowFlags() | Qt::MSWindowsFixedSizeDialogHint);
    progress.setWindowModality(Qt::WindowModal);
    progress.show();

    int photoDlgRes = -1;
    foreach(file, fileNames) {
        progress.setValue(fileNames.indexOf(file));
        double lat = 0.0, lon = 0.0;

        if (!QFile::exists(file)) {
            WARNING(tr("No such file"), tr("Can't find image \"%1\".").arg(file))
            continue;
        }

        try {
            image = Exiv2::ImageFactory::open(file.toStdString());
        }
        catch (Exiv2::Error error)
            WARNING(tr("Exiv2"), tr("Error while opening \"%2\":\n%1").arg(error.what()).arg(file))
        if (image.get() == nullptr)
            WARNING(tr("Exiv2"), tr("Error while loading EXIF-data from \"%1\".").arg(file))

        image->readMetadata();
        exifData = image->exifData();

        time = QDateTime();
        if (!exifData.empty()) {
            Exiv2::Exifdatum &latV = exifData["Exif.GPSInfo.GPSLatitude"];
            Exiv2::Exifdatum &lonV = exifData["Exif.GPSInfo.GPSLongitude"];
            positionValid = latV.count()==3 && lonV.count()==3;

            if (positionValid) {
                lat = latV.toFloat(0) + latV.toFloat(1) / 60.0 + latV.toFloat(2) / 3600.0;
                lon = lonV.toFloat(0) + lonV.toFloat(1) / 60.0 + lonV.toFloat(2) / 3600.0;
                if (exifData["Exif.GPSInfo.GPSLatitudeRef"].toString() == "S")
                    lat *= -1.0;
                if (exifData["Exif.GPSInfo.GPSLongitudeRef"].toString() == "W")
                    lon *= -1.0;
            }

            QString timeStamp = QString::fromStdString(exifData["Exif.Image.DateTime"].toString());
            if (timeStamp.isEmpty())
                timeStamp = QString::fromStdString(exifData["Exif.Photo.DateTimeOriginal"].toString());

            if (!timeStamp.isEmpty())
                time = QDateTime::fromString(timeStamp, "yyyy:MM:dd hh:mm:ss");
        }
//        if (exifData.empty() || (!positionValid && time.isNull()) ) {
//            // this question is asked when the file timestamp is used to find out to which node the image belongs
//            QUESTION(tr("No EXIF"), tr("No EXIF header found in image \"%1\".\nDo you want to revert to improper file timestamp?").arg(file), timeQuestion);
//            time = QFileInfo(file).created();
//        }
        if (time.isNull()) // if time is still nullptr, we use the file date as reference for image sorting (and not for finding out to which node the image belongs)
            // so we don't have to ask a question here
            time = QFileInfo(file).created();

        int res = photoDlgRes;
        if (!positionValid && res == -1) {
            QDialog* dlg = new QDialog;
            Ui::PhotoLoadErrorDialog* ui = new Ui::PhotoLoadErrorDialog;
            ui->setupUi(dlg);
            ui->photo->setPixmap(QPixmap(file).scaledToWidth(320));

            if (M_PREFS->getOfflineMode())
                ui->pbBarcode->setVisible(false);

            dlg->exec();
            if (ui->pbIgnore->isChecked())
                res = 0;
            else if (ui->pbLoad->isChecked())
                res = 1;
            else if (ui->pbMatch->isChecked())
                res = 2;
            else if (ui->pbBarcode->isChecked())
                res = 3;

            if (ui->cbRemember->isChecked())
                photoDlgRes = res;
        }

        if (res == 3) {    // Barcode
            QImage img(file);

#ifdef USE_ZBAR
            zbar::QZBarImage image(img);

            // create a reader
            zbar::ImageScanner scanner;

            // configure the reader
            scanner.set_config(zbar::ZBAR_NONE, zbar::ZBAR_CFG_ENABLE, 1);

            // scan the image for barcodes
            scanner.recycle_image(image);
            zbar::Image tmp = image.convert(*(long*)"Y800");
            int n = scanner.scan(tmp);
            image.set_symbols(tmp.get_symbols());

            if (n <= 0) {
                if (!askAndgetWalkingPapersDetails(lat, lon, positionValid))
                    continue;
            } else {
                QUrl url;
                // extract results
                for(zbar::Image::SymbolIterator symbol = image.symbol_begin(); symbol != image.symbol_end(); ++symbol) {
                    // do something useful with results
                    qDebug() << "decoded " << QString::fromStdString(symbol->get_type_name())
                            << " symbol \"" << QString::fromStdString(symbol->get_data()) << '"';
                    qDebug() << "x;y: " << symbol->get_location_x(0) << ", " << symbol->get_location_y(0);

                    url = QUrl(QString::fromStdString(symbol->get_data()));
                    if (url.host().contains("walking-papers.org")) {

                        int x = symbol->get_location_x(0);
                        int y = symbol->get_location_y(0);
                        QPoint mid = QPoint(img.width()/2, img.height()/2);
                        if (x < mid.x() || y < mid.y()) {
                            if (QMessageBox::warning(this, "Wrong image orientation", "Image appear to be wrognly oriented.\nDo you want to rotate it?",
                                                     QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes) == QMessageBox::Yes) {
                                QMatrix mat;
                                if (x < mid.x() && y < mid.y())
                                    mat.rotate(180);
                                else if (x > mid.x() && y < mid.y())
                                    mat.rotate(90);
                                else if (x < mid.x() && y > mid.y())
                                    mat.rotate(-90);
                                img = img.transformed(mat);
                                img.save(file);
                            }
                        }
                        getWalkingPapersDetails(url, lat, lon, positionValid);
                    } else {
                        if (!askAndgetWalkingPapersDetails(lat, lon, positionValid))
                            continue;
                    }
                }
            }

            // clean up
            image.set_data(nullptr, 0);
#else
            if (!askAndgetWalkingPapersDetails(lat, lon, positionValid))
                continue;
#endif
        }

        if (positionValid) {
            Coord newPos(lon, lat);
            Node *Pt = 0;
            int i = 0;
            for (; i<theLayer->size(); ++i) { // use existing TrackPoint if there is one in small distance
                if ((Pt = CAST_NODE(theLayer->get(i))) &&
                 Pt->position().distanceFrom(newPos) <= .002)
                    break;
                else
                    Pt = 0;
            }

            PhotoNode* phNode;
            if (!Pt) {
                phNode = g_backend.allocPhotoNode(theLayer, newPos);
                theLayer->add(phNode);
                g_backend.sync(phNode);
            } else {
                if (CAST_TRACKNODE(Pt))
                    phNode = g_backend.allocPhotoNode(theLayer, *CAST_TRACKNODE(Pt));
                else
                    phNode = g_backend.allocPhotoNode(theLayer, *Pt);
                theLayer->add(phNode);
                for (int i=0; i<Pt->sizeParents(); ++i) {
                    Feature *P = CAST_FEATURE(Pt->getParent(i));
                    int idx = P->find(Pt);
                    if (Way* W = CAST_WAY(P)) {
                        W->add(phNode, idx);
                    } else if (Relation* R = CAST_RELATION(P)) {
                        R->add(R->getRole(idx), phNode, idx);
                    } else if (TrackSegment* S = CAST_SEGMENT(P)) {
                        S->add(phNode, idx);
                    }
                    P->remove(idx+1);
                    g_backend.deallocFeature(theLayer, Pt);

                }

            }
                        //Pt->setTag("_waypoint_", "true");
            phNode->setTag("_picture_", "GeoTagged");
            phNode->setPhoto(QPixmap(file));
            addUsedTrackpoint(NodeData(phNode, file, time, i == theLayer->size()));
        } else if (!time.isNull() && res == 2) {

            if (offset == -1) { // ask the user to specify an offset for the images
                QDialog dialog(this);
                dialog.setWindowTitle(tr("Specify offset"));

                QLabel position(tr("Position images more to the:"), &dialog);
                QRadioButton positive(tr("end of the track"), &dialog);
                QRadioButton negative(tr("beginning of the track"), &dialog);
                QTimeEdit timeEdit(&dialog);
                QDialogButtonBox buttons(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, Qt::Horizontal, &dialog);

                timeEdit.setDisplayFormat(tr("hh:mm:ss"));

                connect(&buttons, SIGNAL(accepted()), &dialog, SLOT(accept()));
                connect(&buttons, SIGNAL(rejected()), &dialog, SLOT(reject()));

                QVBoxLayout layout(&dialog); // very important to first declare the QVBoxLayout.
                QHBoxLayout radioLayout; // otherwise there would be a segmentation fault when return;
                QHBoxLayout timeLayout;

                radioLayout.addWidget(&positive);
                radioLayout.addWidget(&negative);
                timeLayout.addStretch();
                timeLayout.addWidget(&timeEdit); // center and make as small as possible
                timeLayout.addStretch();

                layout.addWidget(&position);
                layout.addLayout(&radioLayout);
                layout.addLayout(&timeLayout);
                layout.addWidget(&buttons);

                dialog.setLayout(&layout);

                if (dialog.exec()) { // we have to change the sign here because secsTo returns negative value
                    if (positive.isChecked())
                        offset = - timeEdit.time().secsTo(QTime(0, 0, 0));
                    else if (negative.isChecked())
                        offset = timeEdit.time().secsTo(QTime(0, 0, 0));
                    else
                        offset = 0;
                } else {
                    theView->invalidate(true, true, false);
                    if (photoLayer && !photoLayer->size()) {
                        theDocument->remove(photoLayer);
                        SAFE_DELETE(photoLayer);
                    }
                    return;
                }
            }

            time = time.addSecs(offset);

            Feature *feature = nullptr;
            TrackNode *Pt, *bestPt = nullptr;
            int a, secondsTo = INT_MAX;
            int u;

            for (u=0; u<theLayer->size(); u++) {
                feature = theLayer->get(u);
                if ((Pt = dynamic_cast<TrackNode*>(feature))) {
                    a = time.secsTo(Pt->time().toLocalTime());
                    if (abs(a) < abs(secondsTo)) {
                        secondsTo = a;
                        bestPt = Pt;
                    }
                }
            }

            if (!bestPt)
                WARNING(tr("No TrackPoints"), tr("No TrackPoints found for image \"%1\""));

            if (abs(secondsTo) >= 15) {
                QTime difference = QTime().addSecs(abs(secondsTo));
                QString display;
                if (difference.hour() == 0)
                    if (difference.minute() == 0)
                        display = difference.toString(tr("ss 'seconds'"));
                    else
                        display = difference.toString(tr("mm 'minutes and' ss 'seconds'"));
                else
                    display = difference.toString(tr("hh 'hours,' mm 'minutes and' ss 'seconds'"));
                QUESTION(tr("Wrong image?"), secondsTo > 0 ?
                 tr("Image \"%1\" was taken %2 before the next trackpoint was recorded.\nDo you still want to use it?").arg(file).arg(display) :
                 tr("Image \"%1\" was taken %2 after the last trackpoint was recorded.\nDo you still want to use it?").arg(file).arg(display),
                 noMatchQuestion)
            }

            addUsedTrackpoint(NodeData(bestPt, file, time, false));
            //bestPt->setTag("_waypoint_", "true");
            bestPt->setTag("_picture_", "GeoTagged");

            time = QDateTime(); // empty time to be nullptr for the next image
        } else if (res == 1) {
            Coord newPos;
            addUsedTrackpoint(NodeData(nullptr, file, time, true));
        }

        if (progress.wasCanceled()) {
            theView->invalidate(true, true, false);
            if (photoLayer && !photoLayer->size()) {
                theDocument->remove(photoLayer);
                SAFE_DELETE(photoLayer)
            }
            return;
        }
        qApp->processEvents();
    }

    progress.setValue(fileNames.size());

    std::sort(usedTrackPoints.begin(), usedTrackPoints.end()); // sort them chronological
    curImage = -1; // the sorting invalidates curImage

    if (photoLayer && !photoLayer->size()) {
        theDocument->remove(photoLayer);
        SAFE_DELETE(photoLayer)
    }
    theView->invalidate(true, true, false);

}

void GeoImageDock::saveImage()
{
    int index = curImage;
    if (index == -1)
        index = lastImage;
    if (index < 0 || index >= usedTrackPoints.size()) { // invalid ImageId
        return;
    }
    Node* n = usedTrackPoints.at(index).node;
    if (!n)
        return;

    QFileInfo fi(usedTrackPoints.at(index).filename);
    QString fn = fi.absoluteFilePath() + "/" + fi.completeBaseName() + ".jpg";

    QFileDialog dlg(nullptr, tr("Specify output filename"), fn, tr("JPEG Images (*.jpg)"));
    dlg.setFileMode(QFileDialog::AnyFile);
    dlg.setDefaultSuffix("jpg");
    dlg.setAcceptMode(QFileDialog::AcceptSave);

    if (dlg.exec()) {
        if (dlg.selectedFiles().size())
            fn = dlg.selectedFiles()[0];
    }
//    fn = QFileDialog::getSaveFileName(0, "Specify output filename", fn, tr("JPEG Images (*.jpg)"));
    qDebug() << fn;
    if (!fn.isEmpty()) {
        Exiv2::Image::AutoPtr imageIn, imageOut;
        Exiv2::ExifData exifData;
        try {
            imageIn = Exiv2::ImageFactory::open(usedTrackPoints.at(index).filename.toStdString());
            imageIn->readMetadata();
            exifData = imageIn->exifData();
        }
        catch (Exiv2::Error error) {}
        QPixmap px(usedTrackPoints.at(index).filename);
        px.save(fn);
        try {
            imageOut = Exiv2::ImageFactory::open(fn.toStdString());
            imageOut->setExifData(exifData);
            imageOut->writeMetadata();
        }
        catch (Exiv2::Error error) {}
        addGeoDataToImage(n->position(), fn);
    }
}

Coord GeoImageDock::getGeoDataFromImage(const QString & file)
{
    Coord pos;
    double lat = 0.0, lon = 0.0;
    std::unique_ptr<Exiv2::Image> image;
    Exiv2::ExifData exifData;
    bool positionValid = false;

    if (!QFile::exists(file)) {
        return pos;
    }

    try {
        image = Exiv2::ImageFactory::open(file.toStdString());
    }
    catch (Exiv2::Error error) {
        return pos;
    }
    if (image.get() == nullptr)
        return pos;

    image->readMetadata();

    exifData = image->exifData();
    if (!exifData.empty()) {
        Exiv2::Exifdatum &latV = exifData["Exif.GPSInfo.GPSLatitude"];
        Exiv2::Exifdatum &lonV = exifData["Exif.GPSInfo.GPSLongitude"];
        positionValid = latV.count()==3 && lonV.count()==3;

        if (!positionValid)
            return pos;
        lat = latV.toFloat(0) + latV.toFloat(1) / 60.0 + latV.toFloat(2) / 3600.0;
        lon = lonV.toFloat(0) + lonV.toFloat(1) / 60.0 + lonV.toFloat(2) / 3600.0;
        if (exifData["Exif.GPSInfo.GPSLatitudeRef"].toString() == "S")
            lat *= -1.0;
        if (exifData["Exif.GPSInfo.GPSLongitudeRef"].toString() == "W")
            lon *= -1.0;
    }
    pos = Coord(lon, lat);
    return pos;
}

void GeoImageDock::addGeoDataToImage(Coord position, const QString & file)
{
    std::unique_ptr<Exiv2::Image> image;

    try {
        image = Exiv2::ImageFactory::open(file.toStdString());
    }
    catch (Exiv2::Error error) {
        QMessageBox::warning(nullptr, tr("Exiv2"), tr("Error while opening \"%1\":\n%2").arg(file).arg(error.what()), QMessageBox::Ok);
        return;
    }
    if (image.get() == nullptr) {
        QMessageBox::warning(nullptr, tr("Exiv2"), tr("Error while loading EXIF-data from \"%1\".").arg(file), QMessageBox::Ok);
        return;
    }

    image->readMetadata();
    Exiv2::ExifData &exifData = image->exifData();

    double lat = fabs(position.y());
    double lon = fabs(position.x());
    int h, m, s;

    QString hourFormat("%1/1 %2/1 %3/100");

    h = int(lon / 1); // translate angle to hours, minutes and seconds
    m = int((lon - h) * 60 / 1);
    s = int((lon - h - m/60.0) * 60 * 60 * 100 / 1); // multiply with 100 because of divider in hourFormat
    Exiv2::ValueType<Exiv2::URational> vlon;
    vlon.read(hourFormat.arg(h).arg(m).arg(s).toStdString()); // fil vlon with string

    h = int(lat / 1); // translate angle to hours, minutes and seconds
    m = int((lat - h) * 60 / 1);
    s = int((lat - h - m/60.0) * 60 * 60 * 100 / 1); // multiply with 100 because of divider in hourFormat
    Exiv2::ValueType<Exiv2::URational> vlat;
    vlat.read(hourFormat.arg(h).arg(m).arg(s).toStdString()); // fill vlat with string

    exifData["Exif.GPSInfo.GPSVersionID"] = "2 0 0 0";

    exifData["Exif.GPSInfo.GPSLatitude"] = vlat;
    if (position.y() < 0)
        exifData["Exif.GPSInfo.GPSLatitudeRef"] = "S";
    else
        exifData["Exif.GPSInfo.GPSLatitudeRef"] = "N";
    exifData["Exif.GPSInfo.GPSLongitude"] = vlon;
    if (position.x() < 0)
        exifData["Exif.GPSInfo.GPSLongitudeRef"] = "W";
    else
        exifData["Exif.GPSInfo.GPSLongitudeRef"] = "E";

    image->writeMetadata(); // store it

    return;
}

void GeoImageDock::changeEvent(QEvent * event)
{
    if (event->type() == QEvent::LanguageChange)
        retranslateUi();
    MDockAncestor::changeEvent(event);
}

void GeoImageDock::retranslateUi()
{
    setWindowTitle(tr("Geo Images"));
    centerAction->setText(tr("Center map"));
    remImagesAction->setText(tr("Remove Images"));
    toClipboardAction->setText(tr("Copy filename to clipboard"));
    nextImageAction->setText(tr("Select next image"));
    previousImageAction->setText(tr("Select previous image"));
    saveImageAction->setText(tr("Save geotagged image..."));
}

// *** ImageView *** //

ImageView::ImageView(QWidget *parent)
    : QWidget(parent)
{
    zoomLevel = 1.0;
}

ImageView::~ImageView()
{
}

void ImageView::setImage(QString filename, bool movable)
{
    name = filename;
    Movable = movable;
    if (!name.isEmpty())
        image.load(name);
    else
        image = QImage();
    area = QRectF(QPoint(0, 0), image.size());
    zoomLevel = 1.0;
    resizeEvent(nullptr);
    update();
}

void ImageView::setMovable(bool movable)
{
    Movable = movable;
}

void ImageView::paintEvent(QPaintEvent * /* e */)
{
    QPainter P(this);

//    P.setRenderHint(QPainter::SmoothPixmapTransform);
    P.drawImage(rect, image, area, Qt::OrderedDither); // draw the image

    QRect text = QFontMetrics(P.font()).boundingRect(name); // calculate size of filename
    text.translate(-text.topLeft()); // move topLeft to (0, 0)
    if (text.width() > width())
        text.setWidth(width()); // max size is width()

    P.fillRect(text, QColor(255, 255, 255, 192)); // draw the text background

    if (text.width() == width()) { // draw a cutting text ("...") in front of the cutted filename
        QRect cutting = QFontMetrics(P.font()).boundingRect("...");
        cutting.translate(-cutting.topLeft()); // move topLeft to (0, 0)
        text.setWidth(width() - cutting.width());
        text.translate(QPoint(cutting.width(), 0));
        P.drawText(cutting, "...");
    }

    P.drawText(text, Qt::AlignRight, name);

    if (!Movable) {
        P.setPen(QPen(Qt::red, 2));
        P.drawRect(rect);
    }
}

void ImageView::resizeEvent(QResizeEvent * /* e */)
{
    if (image.height() == 0 || image.width() == 0) return;
    rect = geometry();
    rect.translate(-rect.topLeft());
    zoom(0); // update zoom
}

void ImageView::mouseDoubleClickEvent(QMouseEvent * /* e */)
{
    if (QApplication::keyboardModifiers() == Qt::ControlModifier)
        zoom(-1);
    else
        zoom(1);
}

void ImageView::mousePressEvent(QMouseEvent * e)
{
    if ((e->button() & Qt::LeftButton) && !Movable) {
        QDrag *drag = new QDrag(this);
        QMimeData *mimeData = new QMimeData;

        QList<QUrl> urls;
        urls << QUrl::fromLocalFile(name);
        mimeData->setUrls(urls);
        drag->setMimeData(mimeData);
        drag->setPixmap(QPixmap::fromImage(image).scaledToWidth(64));

        /* Qt::DropAction dropAction = */ drag->exec();
        return;
    }
    if (e->button() & Qt::RightButton)
        return QWidget::mousePressEvent(e);
    mousePos = e->pos();
}

void ImageView::mouseMoveEvent(QMouseEvent * e)
{
    if (!Movable)
        return QWidget::mouseMoveEvent(e);

    if (geometry().width() == 0 || geometry().height() == 0) return;
    area.translate((double)(mousePos.x() - e->pos().x()) / (double)rect.width() * area.width(),
        (double)(mousePos.y() - e->pos().y()) / (double)rect.height() * area.height());
    mousePos = e->pos();
    update();
}

void ImageView::wheelEvent(QWheelEvent *e)
{
    zoom(e->delta() / 8.0 / 360.0 * 10.0); // one wheel rotation is about 10 steps
}

void ImageView::zoom(double levelStep)
{
    if (name.isEmpty() || !rect.isValid())
        return;

    // zoomValue (in percent) increases/decreases following this function: 100 * sqrt(2)^x
    // round about it results in -> 100% 150% 200% 300% 400% 550% 800% (see zooming values e.g. in gimp)
    double newZoom = zoomLevel * pow(sqrt(2.0), levelStep);
    if (newZoom > 256 || newZoom < 0.8) // only zoom up to 25600 % or down to 80%
        return;

    QRectF oldArea = area;
    QPointF center = area.center();
    area.setWidth(1 / newZoom * image.width());
    area.setHeight(1 / newZoom * image.height());
    double rAspect = (double)rect.height() / (double)rect.width(); // ensure that area has the same aspect as rect has
    double aAspect = (double)area.height() / (double)area.width();
    if (rAspect > aAspect)
        area.setHeight(area.width() * rAspect);
    else if (rAspect < aAspect)
        area.setWidth(area.height() / rAspect);
    area.moveCenter(center);

    if (levelStep > 0 ) {
        QPointF cursor = mapFromGlobal(QCursor::pos());
        QPointF old = cursor / (double)rect.width() * oldArea.width() + oldArea.topLeft(); // map to image coordinates
        QPointF neu = cursor / (double)rect.width() * area.width() + area.topLeft(); // map to image coordinates
        area.translate(old - neu); // ensure that the point under cursor doesn't move
    }
    zoomLevel = newZoom;

    update();
}

