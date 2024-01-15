/*
 * CBFS Connect 2022 Java Edition - Sample Project
 *
 * This sample project demonstrates the usage of CBFS Connect in a 
 * simple, straightforward way. It is not intended to be a complete 
 * application. Error handling and other checks are simplified for clarity.
 *
 * www.callback.com/cbfsconnect
 *
 * This code is subject to the terms and conditions specified in the 
 * corresponding product license agreement which outlines the authorized 
 * usage and restrictions.
 */

import java.io.*;
import java.awt.*;
import java.awt.event.ActionEvent;
import java.awt.event.ActionListener;
import java.io.*;
import java.lang.reflect.Array;
import java.nio.ByteBuffer;
import java.util.*;
import javax.swing.*;
import javax.swing.event.ListSelectionEvent;
import javax.swing.event.ListSelectionListener;
import javax.swing.filechooser.FileFilter;
import java.nio.file.Path;
import java.nio.file.Paths;
import java.util.regex.Pattern;

import cbfsconnect.*;

public class encrypteddrive extends JFrame implements CbfsEventListener {
    private static final String PRODUCT_GUID = "{713CC6CE-B3E2-4fd9-838D-E28F558F6866}";
    private static final int SECTOR_SIZE = 512;
    private static final int BUFFER_SIZE = 64 * 1024;

    private static final int GAP = 6;
    private static final int DGAP = GAP * 2;
    private static final long emptyDate;

    private static final int GENERIC_WRITE = 0x40000000;
    private static final int GENERIC_ALL = 0x10000000;
    private static final int FILE_WRITE_DATA = 0x0002;
    private static final int FILE_WRITE_EA = 0x0010;
    private static final int FILE_WRITE_ATTRIBUTES = 0x0100;
    private static final int FILE_ATTRIBUTE_DIRECTORY = 0x10;
    private static final int FILE_APPEND_DATA = 0x04;

    private static final int ERROR_FILE_NOT_FOUND = 2;
    private static final int ERROR_ACCESS_DENIED = 5;
    private static final int ERROR_INVALID_HANDLE = 6;
    private static final int ERROR_WRITE_FAULT = 29;
    private static final int ERROR_READ_FAULT = 30;
    private static final int ERROR_SHARING_VIOLATION = 32;
    private static final int ERROR_INVALID_PARAMETER = 87;
    private static final int ERROR_PRIVILEGE_NOT_HELD = 1314;

    private static final Pattern DRIVE_LETTER_PATTERN = Pattern.compile("^[A-Za-z]:$");

    private static Cbfs cbfs;
    private String cabFileLocation;
    private String rootDir;
    private byte[] keyMaterial;
    private boolean initialized;
    private boolean driverRunning;
    private boolean storageCreated;
    private boolean mediaMounted;

    private JPanel panelDriver;
    private JLabel labelVersion;
    private JLabel labelStatus;
    private JButton buttonInstall;
    private JButton buttonUinstall;

    private JPanel panelPassword;
    private JPasswordField textPassword;
    private JCheckBox checkShowPassword;

    private JPanel panelRootFolder;
    private JTextField textRootFolder;
    private JButton buttonSelectRootFolder;

    private JPanel panelOperations;
    private JButton buttonCreateStorage;
    private JButton buttonMountMedia;
    private JButton buttonUnmountMedia;
    private JButton buttonDeleteStorage;

    private JPanel panelMountingPoints;
    private JTextField textMountingPoint;
    private JButton buttonAddMountingPoint;
    private JButton buttonRemoveMountingPoint;
    private JScrollPane scrollMountingPoints;
    private JList listMountingPoints;
    private DefaultListModel modelMountingPoints;

    static {
        Calendar utc = Calendar.getInstance(TimeZone.getTimeZone("UTC"));
        utc.set(1601, Calendar.JANUARY, 1, 0, 0, 0);
        utc.clear(Calendar.MILLISECOND);
        emptyDate = utc.getTimeInMillis();
    }
    private encrypteddrive() throws CBFSConnectException {
        super("Encrypted Drive");
        setDefaultCloseOperation(JFrame.EXIT_ON_CLOSE);
        setLayout(null);
        setLocationByPlatform(true);
        setResizable(false);
        setSize(400, 474);
        setVisible(true);

        initializeControls();

        cbfs.setSerializeAccess(true);
        cbfs.setSerializeEvents(1);
        updateDriverStatus();
        updateControls();
        repaint();
    }


    private void buttonInstallClick() {
        JFileChooser chooser = createOpenCabChooser();
        if (chooser.showOpenDialog(this) != JFileChooser.APPROVE_OPTION)
            return;

        cabFileLocation = chooser.getSelectedFile().getParent();
        boolean rebootNeeded;

        try {
            rebootNeeded = cbfs.install(chooser.getSelectedFile().getPath(), PRODUCT_GUID, null,
                    Constants.MODULE_DRIVER, 0) != 0;
            updateDriverStatus();
            updateControls();
        }
        catch (CBFSConnectException err) {
            int errCode = err.getCode();
            if (errCode == ERROR_PRIVILEGE_NOT_HELD)
                JOptionPane.showMessageDialog(this,
                        "Installation requires administrator rights. Please run the app as Administrator",
                        getTitle(), JOptionPane.WARNING_MESSAGE);
            else
                JOptionPane.showMessageDialog(this,
                        "Installation failed.\n" + err.getMessage(), getTitle(), JOptionPane.ERROR_MESSAGE);
            return;
        }

        if (rebootNeeded)
            JOptionPane.showMessageDialog(this,
                    "System restart is needed for the changes to take effect", getTitle(), JOptionPane.WARNING_MESSAGE);
        else
            JOptionPane.showMessageDialog(this,
                    "Driver installed successfully", getTitle(), JOptionPane.INFORMATION_MESSAGE);
    }

    private void buttonUinstallClick() {
        JFileChooser chooser = createOpenCabChooser();
        if (chooser.showOpenDialog(this) != JFileChooser.APPROVE_OPTION)
            return;

        cabFileLocation = chooser.getSelectedFile().getParent();
        boolean rebootNeeded;

        try {
            rebootNeeded = cbfs.uninstall(chooser.getSelectedFile().getPath(), PRODUCT_GUID, null,
                    Constants.UNINSTALL_VERSION_ALL) != 0;
            updateDriverStatus();
            updateControls();
        }
        catch (CBFSConnectException err) {
            int errCode = err.getCode();
            if (errCode == ERROR_PRIVILEGE_NOT_HELD)
                JOptionPane.showMessageDialog(this,
                        "Uninstallation requires administrator rights. Please run the app as Administrator",
                        getTitle(), JOptionPane.WARNING_MESSAGE);
            else
                JOptionPane.showMessageDialog(this,
                        "Uninstallation failed.\n" + err.getMessage(), getTitle(), JOptionPane.ERROR_MESSAGE);
            return;
        }

        if (rebootNeeded)
            JOptionPane.showMessageDialog(this,
                    "System restart is needed for the changes to take effect", getTitle(), JOptionPane.WARNING_MESSAGE);
        else {
            JOptionPane.showMessageDialog(this,
                    "Driver uninstalled successfully", getTitle(), JOptionPane.INFORMATION_MESSAGE);
        }
    }

    private void buttonSelectRootFolderClick() {
        JFileChooser chooser = createBrowseForFolderChooser();
        if (chooser.showOpenDialog(this) != JFileChooser.APPROVE_OPTION)
            return;

        textRootFolder.setText(chooser.getSelectedFile().getPath());
    }

    private void buttonCreateStorageClick() {
        try {
            if (!initialized) {
                cbfs.initialize(PRODUCT_GUID);
                cbfs.setSerializeAccess(true);
                cbfs.setSerializeEvents(1);
                cbfs.addCbfsEventListener(this);
                initialized = true;
            }
            cbfs.createStorage();
            storageCreated = true;
            updateControls();
        }
        catch (Exception err) {
            JOptionPane.showMessageDialog(this,
                    "Failed to create a storage.\n" + err.getMessage(), getTitle(), JOptionPane.ERROR_MESSAGE);
        }
    }

    private void buttonMountMediaClick() {
        if (!getPassword())
            return;

        rootDir = ConvertRelativePathToAbsolute(textRootFolder.getText());
        if (isNullOrEmpty(rootDir)) {
            JOptionPane.showMessageDialog(this, "No root folder specified.", getTitle(), JOptionPane.ERROR_MESSAGE);
            return;
        }
        if (rootDir.endsWith("\\") || rootDir.endsWith("/"))
            rootDir = rootDir.substring(0, rootDir.length() - 1);

        File dir = new File(rootDir);
        if (dir.exists()) {
            if (!dir.isDirectory()) {
                JOptionPane.showMessageDialog(this, "Invalid root folder: it must not be a file",
                        getTitle(), JOptionPane.ERROR_MESSAGE);
                return;
            }
        }
        else {
            if (!dir.mkdirs()) {
                JOptionPane.showMessageDialog(this, "Failed to create the root folder.",
                        getTitle(), JOptionPane.ERROR_MESSAGE);
                return;
            }
        }

        try {
            cbfs.mountMedia(0);
            mediaMounted = true;
            updateControls();
        }
        catch (CBFSConnectException err) {
            JOptionPane.showMessageDialog(this,
                    "Failed to mount media.\n" + err.getMessage(), getTitle(), JOptionPane.ERROR_MESSAGE);
        }
    }

    private boolean getPassword() {
        char[] password = textPassword.getPassword();
        if (password == null || password.length == 0) {
            JOptionPane.showMessageDialog(this, "No password for data encryption/decryption entered",
                    getTitle(), JOptionPane.ERROR_MESSAGE);
            return false;
        }

        keyMaterial = new byte[16];
        // don't get a password using String as it remains in memory until cleared by GC
        byte[] buffer = new String(password).getBytes();
        if (buffer.length >= 16)
            // take first 16 bytes of the password
            System.arraycopy(buffer, 0, keyMaterial, 0, 16);
        else {
            // extend the password to 16 bytes
            for (int i = 0; i < 16; i += buffer.length)
                System.arraycopy(buffer, 0, keyMaterial, i, Math.min(buffer.length, 16 - i));
        }
        // cleanup the temporary buffer
        Arrays.fill(buffer, (byte)0);

        return true;
    }

    private void buttonUnmountMediaClick() {
        try {
            cbfs.unmountMedia(true);
            mediaMounted = false;
            updateControls();
        }
        catch (CBFSConnectException err) {
            JOptionPane.showMessageDialog(this,
                    "Failed to unmount media.\n" + err.getMessage(), getTitle(), JOptionPane.ERROR_MESSAGE);
        }
    }

    private void buttonDeleteStorageClick() {
        try {
            cbfs.deleteStorage(true);
            modelMountingPoints.clear();
            storageCreated = false;
            updateControls();
        }
        catch (CBFSConnectException err) {
            JOptionPane.showMessageDialog(this,
                    "Failed to delete a storage.\n" + err.getMessage(), getTitle(), JOptionPane.ERROR_MESSAGE);
        }
    }

    private void buttonAddMountingPointClick() {
        try {
            String mountingPoint = textMountingPoint.getText();
            if (mountingPoint.length() == 1) {
                mountingPoint += ":";
            }
            mountingPoint = ConvertRelativePathToAbsolute(mountingPoint, true);

            if (isNullOrEmpty(mountingPoint)) {
                JOptionPane.showMessageDialog(this, "Mounting point not specified.", getTitle(), JOptionPane.ERROR_MESSAGE);
                return;
            }

            cbfs.addMountingPoint(mountingPoint, Constants.STGMP_MOUNT_MANAGER, 0);
            updateMountingPoints();
        }
        catch (CBFSConnectException err) {
            JOptionPane.showMessageDialog(this,
                    "Failed to add a mounting point.\n" + err.getMessage(), getTitle(), JOptionPane.ERROR_MESSAGE);
        }
        updateControls();
    }

    private void buttonRemoveMountingPointClick() {
        try {
            int index = listMountingPoints.getSelectedIndex();
            cbfs.removeMountingPoint(index, "", 0, 0);
            updateMountingPoints();
        }
        catch (CBFSConnectException err) {
            JOptionPane.showMessageDialog(this,
                    "Failed to remove a mounting point.\n" + err.getMessage(), getTitle(), JOptionPane.ERROR_MESSAGE);
        }
        updateControls();
    }

    private void checkShowPasswordClick() {
        if (checkShowPassword.isSelected())
            textPassword.setEchoChar('\u0000');
        else
            textPassword.setEchoChar('*');
    }

    private void updateControls() {
        buttonInstall.setEnabled(!driverRunning);
        buttonUinstall.setEnabled(driverRunning && !storageCreated);

        textPassword.setEnabled(driverRunning && !mediaMounted);
        checkShowPassword.setEnabled(driverRunning && !mediaMounted);

        textRootFolder.setEnabled(driverRunning && !mediaMounted);
        buttonSelectRootFolder.setEnabled(driverRunning && !mediaMounted);

        buttonCreateStorage.setEnabled(driverRunning && !storageCreated);
        buttonMountMedia.setEnabled(storageCreated && !mediaMounted);
        buttonUnmountMedia.setEnabled(mediaMounted);
        buttonDeleteStorage.setEnabled(storageCreated && !mediaMounted);

        buttonAddMountingPoint.setEnabled(storageCreated);
        buttonRemoveMountingPoint.setEnabled(listMountingPoints.getSelectedIndex() >= 0);
    }

    private void updateDriverStatus() {
        try {
            DriverStatus status = DriverStatus.fromInt(cbfs.getDriverStatus(PRODUCT_GUID, Constants.MODULE_DRIVER));
            driverRunning = (status == DriverStatus.RUNNING);

            if (status == null) {
                labelVersion.setText("Version: ERROR");
                labelStatus.setText("Status: Failed to get driver status");
            }
            else
            if (status == DriverStatus.NOT_INSTALLED) {
                labelVersion.setText("Version: not installed");
                labelStatus.setText("Service: not installed");
            }
            else {
                long version = cbfs.getModuleVersion(PRODUCT_GUID, Constants.MODULE_DRIVER);
                labelVersion.setText(String.format("Version: %d.%d.%d.%d",
                        version >> 48, (version >> 32) & 0xffff, (version >> 16) & 0xffff, version & 0xffff));
                labelStatus.setText(String.format("Service: %s", status));
            }
        }
        catch (CBFSConnectException err) {
            labelVersion.setText("Version: ERROR");
            labelStatus.setText(err.getMessage());
            driverRunning = false;
        }
    }

    private void updateMountingPoints() {
        modelMountingPoints.clear();
        MountingPointList mountingPoints = cbfs.getMountingPoints();
        for (int i = 0; i < mountingPoints.size(); i++)
            modelMountingPoints.addElement(mountingPoints.item(i).getName());
    }

    private void initializeControls() {
        Insets frameArea = getClientArea(this);

        panelDriver = new JPanel(null);
        panelDriver.setBorder(BorderFactory.createTitledBorder(" Driver "));
        panelDriver.setLocation(GAP, GAP);
        panelDriver.setSize(frameArea.right - frameArea.left - DGAP, 95);
        add(panelDriver);
        Insets panelArea = getClientArea(panelDriver);

        buttonInstall = new JButton("Install...");
        buttonInstall.setBounds(panelArea.right - GAP - 95, panelArea.top + GAP, 95, 25);
        buttonInstall.addActionListener(new ActionListener() {
            public void actionPerformed(ActionEvent actionEvent) {
                buttonInstallClick();
            }
        });
        panelDriver.add(buttonInstall);

        buttonUinstall = new JButton("Uninstall...");
        buttonUinstall.setBounds(panelArea.right - GAP - 95, getBottom(buttonInstall) + GAP, 95, 25);
        buttonUinstall.addActionListener(new ActionListener() {
            public void actionPerformed(ActionEvent actionEvent) {
                buttonUinstallClick();
            }
        });
        panelDriver.add(buttonUinstall);

        labelVersion = new JLabel("Version: not installed");
        labelVersion.setLocation(panelArea.left + GAP, panelArea.top + GAP);
        labelVersion.setSize(buttonInstall.getX() - labelVersion.getX() - GAP, 18);
        panelDriver.add(labelVersion);

        labelStatus = new JLabel("Service: not installed");
        labelStatus.setLocation(panelArea.left + GAP, labelVersion.getY() + labelVersion.getHeight() + GAP);
        labelStatus.setSize(buttonInstall.getX() - labelVersion.getX() - GAP, 18);
        panelDriver.add(labelStatus);

        panelPassword = new JPanel(null);
        panelPassword.setBorder(BorderFactory.createTitledBorder(" Data Password "));
        panelPassword.setLocation(GAP, getBottom(panelDriver) + GAP);
        panelPassword.setSize(panelDriver.getWidth(), 84);
        add(panelPassword);
        panelArea = getClientArea(panelPassword);

        textPassword = new JPasswordField("cool*p4ssw0rd");
        textPassword.setBounds(panelArea.left + GAP, panelArea.top + GAP,
                panelArea.right - panelArea.left - DGAP, 25);
        textPassword.setEchoChar('\u0000');
        panelPassword.add(textPassword);

        checkShowPassword = new JCheckBox("Show password", true);
        checkShowPassword.setBounds(panelArea.left + GAP, getBottom(textPassword) + GAP,
                textPassword.getWidth(), 16);
        checkShowPassword.addActionListener(new ActionListener() {
            @Override
            public void actionPerformed(ActionEvent actionEvent) {
                checkShowPasswordClick();
            }
        });
        panelPassword.add(checkShowPassword);

        panelRootFolder = new JPanel(null);
        panelRootFolder.setBorder(BorderFactory.createTitledBorder(" Root Folder "));
        panelRootFolder.setLocation(GAP, getBottom(panelPassword) + GAP);
        panelRootFolder.setSize(panelPassword.getWidth(), 64);
        add(panelRootFolder);
        panelArea = getClientArea(panelRootFolder);

        textRootFolder = new JTextField();
        textRootFolder.setLocation(panelArea.left + GAP, panelArea.top + GAP);
        textRootFolder.setText("C:\\DriveRoot");
        panelRootFolder.add(textRootFolder);

        buttonSelectRootFolder = new JButton("...");
        buttonSelectRootFolder.setSize(25, 25);
        buttonSelectRootFolder.setLocation(panelArea.right - GAP - buttonSelectRootFolder.getWidth(), textRootFolder.getY());
        buttonSelectRootFolder.addActionListener(new ActionListener() {
            public void actionPerformed(ActionEvent actionEvent) {
                buttonSelectRootFolderClick();
            }
        });
        panelRootFolder.add(buttonSelectRootFolder);
        textRootFolder.setSize(buttonSelectRootFolder.getX() - textRootFolder.getX() - GAP, 25);

        panelOperations = new JPanel(null);
        panelOperations.setBorder(BorderFactory.createTitledBorder(" Operations "));
        panelOperations.setLocation(GAP, getBottom(panelRootFolder) + GAP);
        panelOperations.setSize(160,
                frameArea.bottom - panelOperations.getY() - frameArea.top - GAP);
        add(panelOperations);
        panelArea = getClientArea(panelOperations);

        buttonCreateStorage = new JButton("1. Create Storage");
        buttonCreateStorage.setBounds(panelArea.left + GAP, panelArea.top + GAP,
                panelArea.right - panelArea.left - DGAP, 25);
        buttonCreateStorage.addActionListener(new ActionListener() {
            public void actionPerformed(ActionEvent actionEvent) {
                buttonCreateStorageClick();
            }
        });
        panelOperations.add(buttonCreateStorage);

        buttonMountMedia = new JButton("2. Mount Media");
        buttonMountMedia.setBounds(panelArea.left + GAP, getBottom(buttonCreateStorage) + GAP,
                buttonCreateStorage.getWidth(), buttonCreateStorage.getHeight());
        buttonMountMedia.addActionListener(new ActionListener() {
            public void actionPerformed(ActionEvent actionEvent) {
                buttonMountMediaClick();
            }
        });
        panelOperations.add(buttonMountMedia);

        buttonUnmountMedia = new JButton("5. Unmount Media");
        buttonUnmountMedia.setBounds(panelArea.left + GAP, getBottom(buttonMountMedia) + GAP * 4,
                buttonCreateStorage.getWidth(), buttonCreateStorage.getHeight());
        buttonUnmountMedia.addActionListener(new ActionListener() {
            public void actionPerformed(ActionEvent actionEvent) {
                buttonUnmountMediaClick();
            }
        });
        panelOperations.add(buttonUnmountMedia);

        buttonDeleteStorage = new JButton("6. Delete Storage");
        buttonDeleteStorage.setBounds(panelArea.left + GAP, getBottom(buttonUnmountMedia) + GAP,
                buttonCreateStorage.getWidth(), buttonCreateStorage.getHeight());
        buttonDeleteStorage.addActionListener(new ActionListener() {
            public void actionPerformed(ActionEvent actionEvent) {
                buttonDeleteStorageClick();
            }
        });
        panelOperations.add(buttonDeleteStorage);

        panelMountingPoints = new JPanel(null);
        panelMountingPoints.setBorder(BorderFactory.createTitledBorder(" Mounting Points "));
        panelMountingPoints.setLocation(getRight(panelOperations) + GAP, getBottom(panelRootFolder) + GAP);
        panelMountingPoints.setSize(getRight(panelRootFolder) - getRight(panelOperations) - GAP, panelOperations.getHeight());
        add(panelMountingPoints);
        panelArea = getClientArea(panelMountingPoints);

        textMountingPoint = new JTextField();
        textMountingPoint.setBounds(panelArea.left + GAP, panelArea.top + GAP,
                panelArea.right - panelArea.left - DGAP, 25);
        textMountingPoint.setText("Z:");
        panelMountingPoints.add(textMountingPoint);

        buttonAddMountingPoint = new JButton("3. Add");
        buttonAddMountingPoint.setBounds(panelArea.left + GAP, getBottom(textMountingPoint) + GAP,
                (textMountingPoint.getWidth() - GAP) / 2, 25);
        buttonAddMountingPoint.addActionListener(new ActionListener() {
            public void actionPerformed(ActionEvent actionEvent) {
                buttonAddMountingPointClick();
            }
        });
        panelMountingPoints.add(buttonAddMountingPoint);

        buttonRemoveMountingPoint = new JButton("4. Remove");
        buttonRemoveMountingPoint.setBounds(getRight(buttonAddMountingPoint) + GAP, buttonAddMountingPoint.getY(),
                getRight(textMountingPoint) - getRight(buttonAddMountingPoint) - GAP, 25);
        buttonRemoveMountingPoint.addActionListener(new ActionListener() {
            public void actionPerformed(ActionEvent actionEvent) {
                buttonRemoveMountingPointClick();
            }
        });
        panelMountingPoints.add(buttonRemoveMountingPoint);

        modelMountingPoints = new DefaultListModel();
        listMountingPoints = new JList(modelMountingPoints);
        listMountingPoints.setSelectionMode(ListSelectionModel.SINGLE_SELECTION);
        listMountingPoints.addListSelectionListener(new ListSelectionListener() {
            public void valueChanged(ListSelectionEvent e) {
                buttonRemoveMountingPoint.setEnabled(listMountingPoints.getSelectedIndex() >= 0);
            }
        });
        JScrollPane scrollMountingPoints = new JScrollPane(listMountingPoints);
        scrollMountingPoints.setLocation(panelArea.left + GAP, getBottom(buttonAddMountingPoint) + GAP);
        scrollMountingPoints.setSize(textMountingPoint.getWidth(), panelArea.bottom - getBottom(buttonAddMountingPoint) - DGAP);
        panelMountingPoints.add(scrollMountingPoints);
    }

    private Insets getClientArea(JComponent c) {
        Rectangle b = c.getBounds();
        Insets i = c.getInsets();
        return new Insets(i.top, i.left,b.height - i.bottom,b.width - i.right);
    }

    private Insets getClientArea(JFrame f) {
        Rectangle b = f.getBounds();
        Insets i = f.getInsets();
        return new Insets(i.top, i.left,b.height - i.bottom,b.width - i.right);
    }

    private int getBottom(JComponent c) {
        return c.getY() + c.getHeight();
    }

    private int getRight(JComponent c) {
        return c.getX() + c.getWidth();
    }

    private boolean isDriveLetter(String path) {
        if (isNullOrEmpty(path)) {
            return false;
        }
        return DRIVE_LETTER_PATTERN.matcher(path).matches();
    }

    private String ConvertRelativePathToAbsolute(String path) {
        return ConvertRelativePathToAbsolute(path, false);
    }

    private String ConvertRelativePathToAbsolute(String path, boolean acceptMountingPoint) {
        String res = null;
        if (path != null && !path.isEmpty()) {
            res = path;

            // Linux-specific case of using a home directory
            if (path.equals("~") || path.startsWith("~/")) {
                String homeDir = System.getProperty("user.home");
                if (path.equals("~") || path.equals("~/"))
                    return homeDir;
                else
                    return Paths.get(homeDir, path.substring(2)).toString();
            }

            int semicolonCount = path.split(";").length - 1;
            boolean isNetworkMountingPoint = semicolonCount == 2;
            String remainedPath = "";

            if (isNetworkMountingPoint) {
                if (!acceptMountingPoint) {
                    JOptionPane.showMessageDialog(this,
                            "The path '" + path + "' format cannot be equal to the Network Mounting Point", getTitle(), JOptionPane.ERROR_MESSAGE);
                    return path;
                }
                int pos = path.indexOf(';');
                if (pos != path.length() - 1) {
                    res = path.substring(0, pos);
                    if (res.isEmpty()) {
                        return path;
                    }
                    remainedPath = path.substring(pos);
                }
            }

            if (isDriveLetter(res)) {
                if (!acceptMountingPoint) {
                    JOptionPane.showMessageDialog(this,
                            "The path '" + res + "' format cannot be equal to the Drive Letter", getTitle(), JOptionPane.ERROR_MESSAGE);
                }
                return path;
            }

            if (!Paths.get(res).isAbsolute()) {
                res = Paths.get(res).toAbsolutePath().toString();
                return res + remainedPath;
            }
        }
        return path;
    }

    private boolean isNullOrEmpty(String s) {
        return (s == null) || (s.length() == 0);
    }

    private boolean isNullOrEmpty(Date value) {
        return (value == null) || (value.getTime() == emptyDate);
    }

    private static boolean isBitSet(int value, int bit) {
        return (value & bit) != 0;
    }

    private boolean isWritingRequested(int access) {
        return isBitSet(access, GENERIC_WRITE) || isBitSet(access, GENERIC_ALL) ||
                isBitSet(access, FILE_WRITE_DATA) || isBitSet(access, FILE_WRITE_EA) ||
                isBitSet(access, FILE_WRITE_ATTRIBUTES) || isBitSet(access, FILE_APPEND_DATA) ;
    }

    private JFileChooser createOpenCabChooser() {
        JFileChooser chooser = new JFileChooser();
        chooser.setDialogTitle("Select CBFS driver package");
        chooser.setMultiSelectionEnabled(false);
        chooser.setFileFilter(new CabFileFilter());
        chooser.setAcceptAllFileFilterUsed(false);
        if (!isNullOrEmpty(cabFileLocation))
            chooser.setCurrentDirectory(new File(cabFileLocation));
        return chooser;
    }

    private JFileChooser createBrowseForFolderChooser() {
        JFileChooser chooser = new JFileChooser();
        chooser.setFileSelectionMode(JFileChooser.DIRECTORIES_ONLY);
        String rootFolderPath = ConvertRelativePathToAbsolute(textRootFolder.getText());
        if (!isNullOrEmpty(rootFolderPath)) {
            File rootFolder = new File(rootFolderPath);
            if (rootFolder.exists() && rootFolder.isDirectory()) {
                chooser.setCurrentDirectory(rootFolder);
            }
        }
        return chooser;
    }

    public static void main(String[] args) throws CBFSConnectException {
        try
        {
            cbfs = new Cbfs();
        }
        catch (UnsatisfiedLinkError ex)
        {
            System.out.println("The JNI library could not be found or loaded.");
            System.out.println("Please refer to the Deployment\\User Mode Library topic for the instructions related to placing the JNI library in the system.");
            System.out.println("Exception details follow:");
            System.out.println(ex.getMessage());
            return;
        }
        new encrypteddrive();
    }

    // -----------------------------------
    // Implementation of CbfsEventListener

    public void canFileBeDeleted(CbfsCanFileBeDeletedEvent e) {
        e.canBeDeleted = true;
    }

    public void cleanupFile(CbfsCleanupFileEvent e) {
        // not needed in this demo
    }

    public void closeDirectoryEnumeration(CbfsCloseDirectoryEnumerationEvent e) {
        if (e.enumerationContext != 0) {
            globals.free(e.enumerationContext);
            e.enumerationContext = 0;
        }
    }

    public void closeFile(CbfsCloseFileEvent e) {
        // because FireAllOpenCloseEvents property is false by default, the event
        // handler will be called only once when all the handles for the file
        // are closed; so it's not needed to deal with file opening counter in this demo
        if (e.fileContext != 0) {
            FileContext context = (FileContext)globals.free(e.fileContext);
            e.fileContext = 0;
            if (context != null) {
                context.clear();
                globals.free(e.fileContext);
            }
        }
    }

    public void closeHardLinksEnumeration(CbfsCloseHardLinksEnumerationEvent e) {
        // out of the demo's scope
    }

    public void closeNamedStreamsEnumeration(CbfsCloseNamedStreamsEnumerationEvent e) {
        // out of the demo's scope
    }

    public void closeQuotasEnumeration(CbfsCloseQuotasEnumerationEvent e) {
        // out of the demo's scope
    }

    public void createFile(CbfsCreateFileEvent e) {
        String name = rootDir + e.fileName;
        File file = new File(name);
        FileContext context = new FileContext(e.attributes);

        if (context.isDirectory()) {
            if (!file.mkdir()) {
                e.resultCode = ERROR_WRITE_FAULT;
                return;
            }
        }
        else {
            try {
                context.file = new RandomAccessFile(file, "rws");
                context.setKeyMaterial(keyMaterial);
            }
            catch (IOException err) {
                context.clear();
                e.resultCode = ERROR_WRITE_FAULT;
                return;
            }
        }

        e.fileContext = globals.alloc(context);
    }

    public void createHardLink(CbfsCreateHardLinkEvent e) {
        // out of the demo's scope
    }

    public void deleteFile(CbfsDeleteFileEvent e) {
        if (e.fileContext != 0) {
            FileContext context = (FileContext) globals.free(e.fileContext);
            if (context != null)
                context.clear();
            e.fileContext = 0;
        }

        String name = rootDir + e.fileName;
        File file = new File(name);
        if (!file.delete())
            e.resultCode = ERROR_WRITE_FAULT;
    }

    public void deleteReparsePoint(CbfsDeleteReparsePointEvent e) {
        // out of the demo's scope
    }

    public void ejected(CbfsEjectedEvent e) {
        // not needed in this demo
    }

    public void enumerateDirectory(CbfsEnumerateDirectoryEvent e) {
        EnumContext context;

        if (e.restart && e.enumerationContext != 0) {
            globals.free(e.enumerationContext);
            e.enumerationContext = 0;
        }

        if (e.enumerationContext == 0) {
            String name = rootDir + e.directoryName;
            File dir = new File(name);
            context = new EnumContext(dir.listFiles(new MaskFileFilter(e.mask)));
            e.enumerationContext = globals.alloc(context);
        }
        else {
            context = (EnumContext)globals.get(e.enumerationContext);
            if (context == null) {
                e.resultCode = ERROR_FILE_NOT_FOUND;
                e.enumerationContext = 0;
                e.fileFound = false;
                return;
            }
        }

        File entry = context.getNext();
        if (entry == null) {
            e.fileFound = false;
        }
        else {
            e.fileFound = true;
            e.fileName = entry.getName();
            e.lastWriteTime = new Date(entry.lastModified());
            if (entry.isDirectory())
                e.attributes = FILE_ATTRIBUTE_DIRECTORY;
            else {
                e.size = entry.length();
                e.allocationSize = e.size;
            }
        }
    }

    public void enumerateHardLinks(CbfsEnumerateHardLinksEvent e) {
        // out of the demo's scope
    }

    public void enumerateNamedStreams(CbfsEnumerateNamedStreamsEvent e) {
        // out of the demo's scope
    }

    public void error(CbfsErrorEvent e) {
        System.err.println(String.format("ERROR: [%d] %s", e.errorCode, e.description));
    }

    public void flushFile(CbfsFlushFileEvent e) {
        // not needed in this demo
    }

    public void fsctl(CbfsFsctlEvent e) {
        // not needed in this demo
    }

    public void getDefaultQuotaInfo(CbfsGetDefaultQuotaInfoEvent e) {
        // out of the demo's scope
    }

    public void getFileInfo(CbfsGetFileInfoEvent e) {
        String name = rootDir + e.fileName;
        File file = new File(name);
        e.fileExists = file.exists();
        if (!e.fileExists)
            return;
        if (file.isDirectory())
            e.attributes |= 0x10;   // FILE_ATTRIBUTE_DIRECTORY
        else {
            e.size = file.length();
            e.allocationSize = e.size;
        }
        e.lastWriteTime = new Date(file.lastModified());
    }

    public void getFileNameByFileId(CbfsGetFileNameByFileIdEvent e) {
        // out of the demo's scope
    }

    public void getFileSecurity(CbfsGetFileSecurityEvent e) {
        // out of the demo's scope
    }

    public void getReparsePoint(CbfsGetReparsePointEvent e) {
        // out of the demo's scope
    }

    public void getVolumeId(CbfsGetVolumeIdEvent e) {
        e.volumeId = 0x12345678;
    }

    public void getVolumeLabel(CbfsGetVolumeLabelEvent e) {
        e.buffer = "Encrypted Drive";
    }

    public void getVolumeObjectId(CbfsGetVolumeObjectIdEvent e) {
        // out of the demo's scope
    }

    public void getVolumeSize(CbfsGetVolumeSizeEvent e) {
        File file = new File(rootDir);
        e.totalSectors = file.getTotalSpace() / SECTOR_SIZE;
        e.availableSectors = file.getFreeSpace() / SECTOR_SIZE;
    }

    public void ioctl(CbfsIoctlEvent e) {
        // not needed in this demo
    }

    public void isDirectoryEmpty(CbfsIsDirectoryEmptyEvent e) {
        String name = rootDir + e.directoryName;
        File dir = new File(name);
        String[] files = dir.list();
        e.isEmpty = (files == null) || (files.length == 0);
    }

    public void lockFile(CbfsLockFileEvent e) {
        // not needed in this demo
    }

    public void mount(CbfsMountEvent e) {
        // not needed in this demo
    }

    public void offloadReadFile(CbfsOffloadReadFileEvent e) {
        // not needed in this demo
    }

    public void offloadWriteFile(CbfsOffloadWriteFileEvent e) {
        // not needed in this demo
    }

    public void openFile(CbfsOpenFileEvent e) {
        FileContext context = (FileContext)globals.get(e.fileContext);
        if (context != null) {
            if (context.readOnly && isWritingRequested(e.desiredAccess)) {
                e.resultCode = ERROR_SHARING_VIOLATION;
            }
            return;
        }

        String name = rootDir + e.fileName;
        File file = new File(name);
        if (!file.exists()) {
            e.resultCode = ERROR_FILE_NOT_FOUND;
            return;
        }

        context = new FileContext(e.attributes);
        if (!context.isDirectory()) {
            try {
                context.file = new RandomAccessFile(file, "rws");
            }
            catch (FileNotFoundException err) {
                try {
                    context.file = new RandomAccessFile(file, "r");
                    context.readOnly = true;
                }
                catch (FileNotFoundException err2) {
                    context.clear();
                    e.resultCode = ERROR_ACCESS_DENIED;
                    return;
                }
            }
            context.setKeyMaterial(keyMaterial);
        }
        e.fileContext = globals.alloc(context);
    }

    public void queryQuotas(CbfsQueryQuotasEvent e) {
        // out of the demo's scope
    }

    public void readFile(CbfsReadFileEvent e) {
        FileContext context = (FileContext)globals.get(e.fileContext);
        if (context == null) {
            e.resultCode = ERROR_INVALID_HANDLE;
            return;
        }

        if (context.isDirectory()) {
            e.resultCode = ERROR_FILE_NOT_FOUND;
            return;
        }

        if (context.file == null) {
            e.resultCode = ERROR_INVALID_HANDLE;
            return;
        }

        try {
            if (e.position < 0 || e.position >= context.file.length()) {
                e.resultCode = ERROR_INVALID_PARAMETER;
                return;
            }

            context.file.seek(e.position);

            int bytesLeft = (int)e.bytesToRead;
            byte[] data = new byte[Math.min(bytesLeft, BUFFER_SIZE)];
            while (bytesLeft > 0) {
                int bytesRead = context.file.read(data, 0, Math.min(bytesLeft, data.length));
                context.decrypt(e.position, data, bytesRead);
                e.buffer.put(data, 0, bytesRead);
                e.bytesRead += bytesRead;
                bytesLeft -= bytesRead;
            }
        }
        catch (IOException err) {
            e.resultCode = ERROR_READ_FAULT;
        }
    }

    public void renameOrMoveFile(CbfsRenameOrMoveFileEvent e) {
        String oldName = rootDir + e.fileName;
        String newName = rootDir + e.newFileName;

        File oldFile = new File(oldName);
        File newFile = new File(newName);

        if (newFile.exists()) {
            // in production, it's better to use a 'rename workaround'
            // instead of deleting the target file/directory
            if (!newFile.delete()) {
                e.resultCode = ERROR_WRITE_FAULT;
                return;
            }
        }

        if (!oldFile.renameTo(newFile))
            e.resultCode = ERROR_WRITE_FAULT;
    }

    public void setAllocationSize(CbfsSetAllocationSizeEvent e) {
        // not needed in this demo
    }

    public void setDefaultQuotaInfo(CbfsSetDefaultQuotaInfoEvent e) {
        // out of the demo's scope
    }

    public void setFileSize(CbfsSetFileSizeEvent e) {
        if (e.fileContext == 0) {
            e.resultCode = ERROR_INVALID_HANDLE;
            return;
        }

        FileContext context = (FileContext)globals.get(e.fileContext);
        if (context == null || context.file == null) {
            e.resultCode = ERROR_INVALID_HANDLE;
            return;
        }

        try {
            context.file.setLength(e.size);
        }
        catch (IOException err) {
            e.resultCode = ERROR_WRITE_FAULT;
        }
    }

    public void setFileAttributes(CbfsSetFileAttributesEvent e) {
        // unfortunately, before introducing java.nio in Java 7, JRE's means
        // to handle file attributes were very very limited and allowed to set
        // only "last modified time" for a file
        if (!isNullOrEmpty(e.lastWriteTime)) {
            String name = rootDir + e.fileName;
            File file = new File(name);
            if (!file.setLastModified(e.lastWriteTime.getTime()))
                e.resultCode = ERROR_WRITE_FAULT;
        }
    }

    public void setFileSecurity(CbfsSetFileSecurityEvent e) {
        // out of the demo's scope
    }

    public void setQuotas(CbfsSetQuotasEvent e) {
        // out of the demo's scope
    }

    public void setReparsePoint(CbfsSetReparsePointEvent e) {
        // out of the demo's scope
    }

    public void setValidDataLength(CbfsSetValidDataLengthEvent e) {
        // not needed in this demo
    }

    public void setVolumeLabel(CbfsSetVolumeLabelEvent e) {
        // not needed in this demo
    }

    public void setVolumeObjectId(CbfsSetVolumeObjectIdEvent e) {
        // out of the demo's scope
    }

    public void unlockFile(CbfsUnlockFileEvent e) {
        // not needed in this demo
    }

    public void unmount(CbfsUnmountEvent e) {
        // not needed in this demo
    }

    public void workerThreadCreation(CbfsWorkerThreadCreationEvent e) {
        // not needed in this demo
    }

    public void workerThreadTermination(CbfsWorkerThreadTerminationEvent e) {
        // not needed in this demo
    }

    public void writeFile(CbfsWriteFileEvent e) {
        FileContext context = (FileContext)globals.get(e.fileContext);
        if (context == null) {
            e.resultCode = ERROR_INVALID_HANDLE;
            return;
        }

        if (context.isDirectory()) {
            e.resultCode = ERROR_FILE_NOT_FOUND;
            return;
        }

        if (context.file == null) {
            e.resultCode = ERROR_INVALID_HANDLE;
            return;
        }

        e.bytesWritten = 0;
        long position = e.position;
        int bytesLeft = (int)e.bytesToWrite;
        byte[] data = new byte[Math.min(bytesLeft, BUFFER_SIZE)];

        try {
            context.file.seek(position);

            while (bytesLeft > 0) {
                int part = Math.min(bytesLeft, data.length);
                e.buffer.get(data, 0, part);
                context.encrypt(position, data, part);
                context.file.write(data, 0, part);
                position += part;
                bytesLeft -= part;
                e.bytesWritten += part;
            }
        }
        catch (IOException err) {
            e.resultCode = ERROR_WRITE_FAULT;
        }
    }

    // End of CbfsEventListener implementation
    // --------------------------------------

    private enum DriverStatus {
        NOT_INSTALLED,
        STOPPED,
        START_PENDING,
        STOP_PENDING,
        RUNNING,
        CONTINUE_PENDING,
        PAUSE_PENDING,
        PAUSED;

        public static DriverStatus fromInt(int value) {
            switch (value) {
                case 0:
                    return NOT_INSTALLED;
                case 1:
                    return STOPPED;
                case 2:
                    return START_PENDING;
                case 3:
                    return STOP_PENDING;
                case 4:
                    return RUNNING;
                case 5:
                    return CONTINUE_PENDING;
                case 6:
                    return PAUSE_PENDING;
                case 7:
                    return PAUSED;
                default:
                    return null;
            }
        }

        @Override
        public String toString() {
            switch (this) {
                case NOT_INSTALLED:
                    return "not installed";
                case STOPPED:
                    return "stopped";
                case START_PENDING:
                    return "start pending";
                case STOP_PENDING:
                    return "stop pending";
                case RUNNING:
                    return "running";
                case CONTINUE_PENDING:
                    return "continue pending";
                case PAUSE_PENDING:
                    return "pause pending";
                case PAUSED:
                    return "paused";
                default:
                    return super.toString();
            }
        }
    }

    private class CabFileFilter extends FileFilter {

        @Override
        public boolean accept(File file) {
            if (file.isDirectory())
                return true;

            return file.getName().equalsIgnoreCase("cbfs.cab");
        }

        @Override
        public String getDescription() {
            return "CBFS Connect driver package";
        }
    }

    private class MaskFileFilter implements FilenameFilter {
        private String mask;

        MaskFileFilter(String mask) {
            this.mask = wildcardToRegex(mask);
        }

        @Override
        public boolean accept(File dir, String name) {
            return name.matches(mask);
        }

        private String wildcardToRegex(String wildcard){
            StringBuilder s = new StringBuilder(wildcard.length());
            s.append('^');
            for (int i = 0, len = wildcard.length(); i < len; i++) {
                char c = wildcard.charAt(i);
                switch(c) {
                    case '*':
                        s.append(".*");
                        break;
                    case '?':
                        s.append(".");
                        break;
                    case '^': // escape character in cmd.exe
                        s.append("\\");
                        break;
                    // escape special regexp-characters
                    case '(': case ')': case '[': case ']': case '$':
                    case '.': case '{': case '}': case '|':
                    case '\\':
                        s.append("\\");
                        s.append(c);
                        break;
                    default:
                        s.append(c);
                        break;
                }
            }
            s.append('$');
            return s.toString();
        }
    }

    private static class FileContext {
        int attributes;
        boolean readOnly;
        byte[] keyMaterial;
        byte[] mask;

        RandomAccessFile file;

        FileContext(int attributes) {
            this.attributes = attributes;
        }

        void clear() {
            attributes = 0;
            if (keyMaterial != null) {
                Arrays.fill(keyMaterial, (byte)0);
                keyMaterial = null;
            }
            if (mask != null) {
                Arrays.fill(mask, (byte)0);
                mask = null;
            }
            if (file != null) {
                try {
                    file.close();
                } catch (IOException err) {
                    // just ignore the error
                }
                file = null;
            }
        }

        void decrypt(long position, byte[] data, int size) {
            encrypt(position, data, size);
        }

        void encrypt(long position, byte[] data, int size) {
            if (keyMaterial == null)
                return;

            int blockLen = keyMaterial.length;
            long blockNo = position / blockLen;
            int blockCount = (int)((position - blockNo * blockLen + size + blockLen - 1) / blockLen);

            byte[] iv = Arrays.copyOf(keyMaterial, keyMaterial.length);
            addLong(iv, blockNo);
            for (int i = 0; i < blockCount; i++) {
                try {
                    System.arraycopy(iv, 0, mask, i * blockLen, blockLen);
                }
                catch (ArrayIndexOutOfBoundsException err) {
                    throw err;
                }
                addOne(iv);
            }

            int offset = (int)(position % blockLen);
            for (int i = 0; i < size; i++)
                data[i] ^= mask[i + offset];
        }

        boolean isDirectory() {
            return isBitSet(attributes, FILE_ATTRIBUTE_DIRECTORY);
        }

        byte[] buffer;

        void setKeyMaterial(byte[] value) {
            if (isDirectory() || value == null)
                return;
            keyMaterial = Arrays.copyOf(value, value.length);
            buffer = new byte[BUFFER_SIZE];
            mask = new byte[BUFFER_SIZE + 16];
        }

        // -----------------------

        private void addLong(byte[] data, long value) {
            ByteBuffer bytes = ByteBuffer.allocate(Long.SIZE / 8);
            bytes.putLong(0, value);

            int a = data.length - 8;
            int b = 0;
            for (int i = data.length - 1; i >= 0; i--) {
                if (i > a)
                    b += bytes.get(i - a) & 0xff;
                b += data[i] & 0xff;
                data[i] = (byte)(b & 0xff);
                b >>= 8;
            }
        }

        private void addOne(byte[] data) {
            int b = 1;
            for (int i = data.length - 1; i >= 0; i--) {
                b += data[i] & 0xff;
                data[i] = (byte)(b & 0xff);
                b >>= 8;
                if (b == 0)
                    break;
            }
        }
    }

    private static class EnumContext {
        private File[] files;
        private int current;

        EnumContext(File[] files) {
            this.files = files;
            current = 0;
        }

        File getNext() {
            if (files == null || current == files.length)
                return null;
            return files[current++];
        }
    }


    /**
     * The storage for global objects (such as contexts) which are returned to a driver
     * and must not be processed by GC until explicitly released.
     */
    private static class globals {
        private static long counter = 0;
        private static final Map<Long, Item> storage = new HashMap<Long, Item>();

        /**
         * Increases the reference counter for a specified global object by its ID.
         * @param id    ID of a stored object to increase the reference counter
         * @return A stored object that corresponds to the specified ID,
         * or {@code null} if no object with the specified ID exists.
         * @see globals#release(long)
         */
        static Object acquire(long id) {
            synchronized (storage) {
                Item item = storage.get(id);
                if (item == null)
                    return null;
                item.refs++;
                return item.target;
            }
        }

        /**
         * Adds a new object and initializes its reference counter to 1.
         * @param target An object to add.
         * @return An ID of the stored object.
         * @see globals#free(long)
         */
        static long alloc(Object target) {
            synchronized (storage) {
                long id = ++counter;
                Item item = new Item(target);
                storage.put(id, item);
                return id;
            }
        }

        /**
         * Removes all the global items from the storage.
         */
        static void clear() {
            synchronized (storage) {
                for (long id: storage.keySet()) {
                    Item item = storage.get(id);
                    item.clear();
                }
                storage.clear();
            }
        }

        /**
         * Removes a stored object by its ID despite the value of its reference counter.
         * @param id ID of an object to remove.
         * @return The removed object, or {@code null} if the specified ID not found.
         * @see globals#alloc(Object)
         */
        static Object free(long id) {
            synchronized (storage) {
                Item item = storage.remove(id);
                if (item == null)
                    return null;
                Object target = item.target;
                item.clear();
                return target;
            }
        }

        /**
         * Gets a stored object by its ID. The object's reference counter is not updated.
         * @param id ID of an object to retrieve.
         * @return A stored object that corresponds to the specified ID,
         * or {@code null} if no object with the specified ID exists in the storage.
         * @see globals#set(long, Object)
         */
        static Object get(long id) {
            synchronized (storage) {
                Item item = storage.get(id);
                return (item == null) ? null : item.target;
            }
        }

        /**
         * Decreases the reference counter of an object by its ID in the storage, and removes the object
         * from the storage if its reference counter become 0.
         * @param id An object ID to release.
         * @return The removed object if its reference counter became 0, or {@code null} if either no object
         * with the specified ID exists in the storage or the reference counter for the object is still greater than 0.
         * @see globals#acquire(long)
         */
        static Object release(long id) {
            synchronized (storage) {
                Item item = storage.get(id);
                if (item == null)
                    return null;
                if (--item.refs > 0)
                    return null;
                storage.remove(id);
                Object target = item.target;
                item.clear();
                return target;
            }
        }

        /**
         * Stores a new object by existing ID. The object's reference counter is not updated.
         * @param id     ID of an object to replace.
         * @param target A new object to store.
         * @return A previously stored object, or {@code null} if either the specified ID not found or
         * the {@code target} is already stored with the specified ID.
         */
        static Object set(long id, Object target) {
            synchronized (storage) {
                Item item = storage.get(id);
                if (item == null)
                    return null;
                if (item.target == target)
                    return null;
                Object old = item.target;
                item.target = target;
                return old;
            }
        }

        private static class Item {
            int refs;
            Object target;

            Item(Object target) {
                super();
                this.refs = 1;
                this.target = target;
            }

            void clear() {
                refs = 0;
                target = null;
            }
        }
    }

}









