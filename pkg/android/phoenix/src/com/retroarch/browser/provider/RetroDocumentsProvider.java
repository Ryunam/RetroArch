// Contains GPLv3-licensed code from the Termux project.
// https://github.com/termux/termux-app/blob/master/app/src/main/java/com/termux/filepicker/TermuxDocumentsProvider.java

package com.retroarch.browser.provider;

import android.annotation.TargetApi;
import android.content.res.AssetFileDescriptor;
import android.database.Cursor;
import android.database.MatrixCursor;
import android.graphics.Point;
import android.os.Build;
import android.os.CancellationSignal;
import android.os.Environment;
import android.os.ParcelFileDescriptor;
import android.provider.DocumentsContract;
import android.provider.DocumentsContract.Document;
import android.provider.DocumentsContract.Root;
import android.provider.DocumentsProvider;
import android.webkit.MimeTypeMap;

import com.retroarch.R;

import java.io.File;
import java.io.FileNotFoundException;
import java.io.IOException;
import java.util.Collections;
import java.util.LinkedList;

/**
 * A document provider for the Storage Access Framework which exposes the files in the
 * $HOME/ folder to other apps.
 * <p/>
 * Note that this replaces providing an activity matching the ACTION_GET_CONTENT intent:
 * <p/>
 * "A document provider and ACTION_GET_CONTENT should be considered mutually exclusive. If you
 * support both of them simultaneously, your app will appear twice in the system picker UI,
 * offering two different ways of accessing your stored data. This would be confusing for users."
 * - http://developer.android.com/guide/topics/providers/document-provider.html#43
 */
@TargetApi(Build.VERSION_CODES.LOLLIPOP)
public class RetroDocumentsProvider extends DocumentsProvider {

    private static final String ALL_MIME_TYPES = "*/*";

    private String DOCUMENTS_AUTHORITY;

    // The default columns to return information about a root if no specific
    // columns are requested in a query.
    private static final String[] DEFAULT_ROOT_PROJECTION = new String[]{
            Root.COLUMN_ROOT_ID,
            Root.COLUMN_MIME_TYPES,
            Root.COLUMN_FLAGS,
            Root.COLUMN_ICON,
            Root.COLUMN_TITLE,
            Root.COLUMN_SUMMARY,
            Root.COLUMN_DOCUMENT_ID,
            Root.COLUMN_AVAILABLE_BYTES
    };

    // The default columns to return information about a document if no specific
    // columns are requested in a query.
    private static final String[] DEFAULT_DOCUMENT_PROJECTION = new String[]{
            Document.COLUMN_DOCUMENT_ID,
            Document.COLUMN_MIME_TYPE,
            Document.COLUMN_DISPLAY_NAME,
            Document.COLUMN_LAST_MODIFIED,
            Document.COLUMN_FLAGS,
            Document.COLUMN_SIZE
    };

    @Override
    public Cursor queryRoots(String[] projection) throws FileNotFoundException {
        final MatrixCursor result = new MatrixCursor(projection != null ? projection : DEFAULT_ROOT_PROJECTION);
        @SuppressWarnings("ConstantConditions") final String applicationName = getContext().getString(R.string.app_name);

        final File CORE_DIR = new File(getContext().getFilesDir().getParent());
        final MatrixCursor.RowBuilder core = result.newRow();
        core.add(Root.COLUMN_ROOT_ID, getDocIdForFile(CORE_DIR));
        core.add(Root.COLUMN_DOCUMENT_ID, getDocIdForFile(CORE_DIR));
        core.add(Root.COLUMN_SUMMARY, "Core Data");
        core.add(Root.COLUMN_FLAGS, Root.FLAG_SUPPORTS_CREATE | Root.FLAG_SUPPORTS_SEARCH | Root.FLAG_SUPPORTS_IS_CHILD);
        core.add(Root.COLUMN_TITLE, applicationName);
        core.add(Root.COLUMN_MIME_TYPES, ALL_MIME_TYPES);
        core.add(Root.COLUMN_AVAILABLE_BYTES, CORE_DIR.getFreeSpace());
        core.add(Root.COLUMN_ICON, R.mipmap.ic_launcher);

        final File USER_DIR = new File(Environment.getExternalStorageDirectory().getAbsolutePath() + "/Android/data/" + getContext().getPackageName() + "/files/RetroArch");
        final MatrixCursor.RowBuilder user = result.newRow();
        user.add(Root.COLUMN_ROOT_ID, getDocIdForFile(USER_DIR));
        user.add(Root.COLUMN_DOCUMENT_ID, getDocIdForFile(USER_DIR));
        user.add(Root.COLUMN_SUMMARY, "User Data");
        user.add(Root.COLUMN_FLAGS, Root.FLAG_SUPPORTS_CREATE | Root.FLAG_SUPPORTS_SEARCH | Root.FLAG_SUPPORTS_IS_CHILD);
        user.add(Root.COLUMN_TITLE, applicationName);
        user.add(Root.COLUMN_MIME_TYPES, ALL_MIME_TYPES);
        user.add(Root.COLUMN_AVAILABLE_BYTES, USER_DIR.getFreeSpace());
        user.add(Root.COLUMN_ICON, R.mipmap.ic_launcher);

        return result;
    }

    @Override
    public Cursor queryDocument(String documentId, String[] projection) throws FileNotFoundException {
        final MatrixCursor result = new MatrixCursor(projection != null ? projection : DEFAULT_DOCUMENT_PROJECTION);
        includeFile(result, documentId, null);
        return result;
    }

    @Override
    public String moveDocument(String sourceDocumentId, String sourceParentDocumentId, String targetParentDocumentId) throws FileNotFoundException {
        File sourceFile = getFileForDocId(sourceDocumentId);
        File targetParentFile = getFileForDocId(targetParentDocumentId);
        File destination = new File(targetParentFile, sourceFile.getName());

        boolean success = sourceFile.renameTo(destination);
        if(!success){
            throw new FileNotFoundException("Failed to move file " + sourceDocumentId + " to " + targetParentDocumentId);
        }

        getContext().getContentResolver().notifyChange(DocumentsContract.buildTreeDocumentUri(DOCUMENTS_AUTHORITY, sourceParentDocumentId), null);
        getContext().getContentResolver().notifyChange(DocumentsContract.buildTreeDocumentUri(DOCUMENTS_AUTHORITY, targetParentDocumentId), null);
        return getDocIdForFile(destination);
    }

    @Override
    public String renameDocument(String documentId, String displayName) throws FileNotFoundException{
        File document = getFileForDocId(documentId);
        File destination = new File(document.getParentFile(), displayName);

        boolean success = document.renameTo(destination);
        if(!success){
            throw new FileNotFoundException("Failed to rename file " + documentId + " to " + displayName);
        }

        getContext().getContentResolver().notifyChange(DocumentsContract.buildTreeDocumentUri(DOCUMENTS_AUTHORITY, getDocIdForFile(document.getParentFile())), null);
        return getDocIdForFile(destination);
    }

    @Override
    public Cursor queryChildDocuments(String parentDocumentId, String[] projection, String sortOrder) throws FileNotFoundException {
        final MatrixCursor result = new MatrixCursor(projection != null ? projection : DEFAULT_DOCUMENT_PROJECTION);
        final File parent = getFileForDocId(parentDocumentId);
        for (File file : parent.listFiles()) {
            includeFile(result, null, file);
        }
        result.setNotificationUri(getContext().getContentResolver(), DocumentsContract.buildTreeDocumentUri(DOCUMENTS_AUTHORITY, parentDocumentId));
        return result;
    }

    @Override
    public ParcelFileDescriptor openDocument(final String documentId, String mode, CancellationSignal signal) throws FileNotFoundException {
        final File file = getFileForDocId(documentId);
        final int accessMode = ParcelFileDescriptor.parseMode(mode);
        return ParcelFileDescriptor.open(file, accessMode);
    }

    @Override
    public AssetFileDescriptor openDocumentThumbnail(String documentId, Point sizeHint, CancellationSignal signal) throws FileNotFoundException {
        final File file = getFileForDocId(documentId);
        final ParcelFileDescriptor pfd = ParcelFileDescriptor.open(file, ParcelFileDescriptor.MODE_READ_ONLY);
        return new AssetFileDescriptor(pfd, 0, file.length());
    }

    @Override
    public boolean onCreate() {
        DOCUMENTS_AUTHORITY = getContext().getPackageName() + ".documents";
        return true;
    }

    @Override
    public String createDocument(String parentDocumentId, String mimeType, String displayName) throws FileNotFoundException {
        File newFile = new File(parentDocumentId, displayName);
        int noConflictId = 2;
        while (newFile.exists()) {
            newFile = new File(parentDocumentId, displayName + " (" + noConflictId++ + ")");
        }
        try {
            boolean succeeded;
            if (Document.MIME_TYPE_DIR.equals(mimeType)) {
                succeeded = newFile.mkdir();
            } else {
                succeeded = newFile.createNewFile();
            }
            if (!succeeded) {
                throw new FileNotFoundException("Failed to create document with id " + newFile.getPath());
            }
        } catch (IOException e) {
            throw new FileNotFoundException("Failed to create document with id " + newFile.getPath());
        }
        getContext().getContentResolver().notifyChange(DocumentsContract.buildTreeDocumentUri(DOCUMENTS_AUTHORITY, parentDocumentId), null);
        return newFile.getPath();
    }

    @Override
    public void deleteDocument(String documentId) throws FileNotFoundException {
        File file = getFileForDocId(documentId);
        rm_r(file);
        getContext().getContentResolver().notifyChange(DocumentsContract.buildTreeDocumentUri(DOCUMENTS_AUTHORITY, getDocIdForFile(file.getParentFile())), null);
    }

    void rm_r (File file) throws FileNotFoundException{
        if(file.isDirectory()){
            for(File child : file.listFiles()) {
                rm_r(child);
            }
        }
        if(!file.delete()){
            throw new FileNotFoundException("Could not delete file " + file.getPath());
        }
    }

    @Override
    public String getDocumentType(String documentId) throws FileNotFoundException {
        File file = getFileForDocId(documentId);
        return getMimeType(file);
    }

    @Override
    public Cursor querySearchDocuments(String rootId, String query, String[] projection) throws FileNotFoundException {
        final MatrixCursor result = new MatrixCursor(projection != null ? projection : DEFAULT_DOCUMENT_PROJECTION);
        final File parent = getFileForDocId(rootId);

        // This example implementation searches file names for the query and doesn't rank search
        // results, so we can stop as soon as we find a sufficient number of matches.  Other
        // implementations might rank results and use other data about files, rather than the file
        // name, to produce a match.
        final LinkedList<File> pending = new LinkedList<>();
        pending.add(parent);

        final int MAX_SEARCH_RESULTS = 50;
        while (!pending.isEmpty() && result.getCount() < MAX_SEARCH_RESULTS) {
            final File file = pending.removeFirst();
            // Avoid folders outside the $HOME folders linked in to symlinks (to avoid e.g. search
            // through the whole SD card).
            boolean isInsideHome;
            try {
                isInsideHome = file.getCanonicalPath().startsWith(getContext().getFilesDir().getParent());
            } catch (IOException e) {
                isInsideHome = true;
            }
            if (isInsideHome) {
                if (file.isDirectory()) {
                    Collections.addAll(pending, file.listFiles());
                } else {
                    if (file.getName().toLowerCase().contains(query)) {
                        includeFile(result, null, file);
                    }
                }
            }
        }

        return result;
    }

    @Override
    public boolean isChildDocument(String parentDocumentId, String documentId) {
        return documentId.startsWith(parentDocumentId);
    }

    /**
     * Get the document id given a file. This document id must be consistent across time as other
     * applications may save the ID and use it to reference documents later.
     * <p/>
     * The reverse of @{link #getFileForDocId}.
     */
    private static String getDocIdForFile(File file) {
        return file.getAbsolutePath();
    }

    /**
     * Get the file given a document id (the reverse of {@link #getDocIdForFile(File)}).
     */
    private static File getFileForDocId(String docId) throws FileNotFoundException {
        final File f = new File(docId);
        if (!f.exists()) throw new FileNotFoundException(f.getAbsolutePath() + " not found");
        return f;
    }

    private static String getMimeType(File file) {
        if (file.isDirectory()) {
            return Document.MIME_TYPE_DIR;
        } else {
            final String name = file.getName();
            final int lastDot = name.lastIndexOf('.');
            if (lastDot >= 0) {
                final String extension = name.substring(lastDot + 1).toLowerCase();
                final String mime = MimeTypeMap.getSingleton().getMimeTypeFromExtension(extension);
                if (mime != null) return mime;
            }
            return "application/octet-stream";
        }
    }

    /**
     * Add a representation of a file to a cursor.
     *
     * @param result the cursor to modify
     * @param docId  the document ID representing the desired file (may be null if given file)
     * @param file   the File object representing the desired file (may be null if given docID)
     */
    private void includeFile(MatrixCursor result, String docId, File file)
            throws FileNotFoundException {
        if (docId == null) {
            docId = getDocIdForFile(file);
        } else {
            file = getFileForDocId(docId);
        }

        int flags = 0;
        if (file.isDirectory()) {
            if (file.canWrite()) flags |= Document.FLAG_DIR_SUPPORTS_CREATE | Document.FLAG_SUPPORTS_RENAME;
        } else {
            if (file.canWrite()) flags |= Document.FLAG_SUPPORTS_WRITE | Document.FLAG_SUPPORTS_RENAME;
        }
        if (file.getParentFile().canWrite()) flags |= Document.FLAG_SUPPORTS_DELETE | Document.FLAG_SUPPORTS_MOVE;

        final String displayName = file.getName();
        final String mimeType = getMimeType(file);
        if (mimeType.startsWith("image/")) flags |= Document.FLAG_SUPPORTS_THUMBNAIL;

        final MatrixCursor.RowBuilder row = result.newRow();
        row.add(Document.COLUMN_DOCUMENT_ID, docId);
        row.add(Document.COLUMN_DISPLAY_NAME, displayName);
        row.add(Document.COLUMN_SIZE, file.length());
        row.add(Document.COLUMN_MIME_TYPE, mimeType);
        row.add(Document.COLUMN_LAST_MODIFIED, file.lastModified());
        row.add(Document.COLUMN_FLAGS, flags);
        row.add(Document.COLUMN_ICON, R.mipmap.ic_launcher);
    }

}
