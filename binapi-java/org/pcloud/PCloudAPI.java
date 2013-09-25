package com.mobileinno.hotfile.abstraction.networking;

import java.io.*;
import java.net.*;
import java.util.*;
import java.nio.*;
import javax.net.ssl.SSLSocket;
import javax.net.ssl.SSLSocketFactory;

class PCloudAPIReader {

    private InputStream istream;
    private int length;
    private byte[] buffer;
    private int bytesInBuffer;
    private int bufferOffset;

    public PCloudAPIReader(InputStream stream) throws IOException {
	istream = stream;
	byte[] len = new byte[4];
	int off = 0;
	while (off < 4) {
	    int res = stream.read(len, off, 4 - off);
	    if (res == -1) {
		throw (new IOException());
	    } else {
		off += res;
	    }
	}
	length = ByteBuffer.wrap(len).order(ByteOrder.LITTLE_ENDIAN).getInt();
	off = 4096;
	if (off > length) {
	    off = length;
	}
	buffer = new byte[off];
	bytesInBuffer = stream.read(buffer, 0, off);
	if (bytesInBuffer == -1) {
	    throw (new IOException());
	}
	length -= bytesInBuffer;
	bufferOffset = 0;
    }

    private void fillBuffer() throws IOException {
	if (length == 0) {
	    throw (new IOException());
	}
	int cnt = 4096;
	if (cnt > length) {
	    cnt = length;
	}
	bytesInBuffer = istream.read(buffer, 0, cnt);
	if (bytesInBuffer == -1) {
	    throw (new IOException());
	}
	length -= bytesInBuffer;
	bufferOffset = 0;
    }

    public int getByte() throws IOException {
	if (bufferOffset >= bytesInBuffer) {
	    fillBuffer();
	}
	return buffer[bufferOffset++] & 0xff;
    }

    public int peekByte() throws IOException {
	if (bufferOffset >= bytesInBuffer) {
	    fillBuffer();
	}
	return buffer[bufferOffset] & 0xff;
    }

    public byte[] getBytes(int cnt) throws IOException {
	byte[] ret = new byte[cnt];
	int off = 0;
	while (off < cnt) {
	    if (bufferOffset >= bytesInBuffer) {
		fillBuffer();
	    }
	    int mb = cnt - off;
	    if (mb > bytesInBuffer - bufferOffset) {
		mb = bytesInBuffer - bufferOffset;
	    }
	    System.arraycopy(buffer, bufferOffset, ret, off, mb);
	    bufferOffset += mb;
	    off += mb;
	}
	return ret;
    }
}

class PCloudAPIStringCollection {

    public int length;
    private int alloced;
    private String[] strings;

    public PCloudAPIStringCollection() {
	length = 0;
	alloced = 32;
	strings = new String[alloced];
    }

    public void add(String s) {
	if (length == alloced) {
	    String[] newstrings = new String[alloced * 2];
	    System.arraycopy(strings, 0, newstrings, 0, alloced);
	    alloced *= 2;
	    strings = newstrings;
	}
	strings[length++] = s;
    }

    public String get(int idx) throws InputMismatchException {
	if (idx >= length) {
	    throw (new InputMismatchException());
	}
	return strings[idx];
    }
}

public class PCloudAPI {

    private Socket sock;
    private static String apihost = "api.pcloud.com";
    private static int apiport = 8398;
    private static int apisslport = 8399;
    private long datalen;

    protected void finalize() throws IOException {
	sock.close();
    }

    public PCloudAPI(boolean ssl) throws UnknownHostException, IOException {
	if (ssl) {
	    sock = SSLSocketFactory.getDefault().createSocket(apihost, apisslport);
	} else {
	    sock = new Socket(apihost, apiport);
	}
	datalen = 0;
    }

    public PCloudAPI() throws UnknownHostException, IOException {
	this(false);
    }

    private byte[] longToByte(long l) {
	return ByteBuffer.allocate(8).order(ByteOrder.LITTLE_ENDIAN).putLong(l).array();
    }

    private long bytesToLong(byte[] b) {
	long l = 0;
	long m = 1;
	for (int i = 0; i < b.length; i++) {
	    l += m * (b[i] & 0xff);
	    m *= 256;
	}
	return l;
    }

    private Object readObject(PCloudAPIReader reader, PCloudAPIStringCollection strings) throws IOException, InputMismatchException {
	int type = reader.getByte();
	if (type >= 8 && type <= 15) {
	    return bytesToLong(reader.getBytes(type - 7));
	} else if (type >= 200 && type < 220) {
	    return (long) (type - 200);
	} else if ((type >= 100 && type < 150) || (type >= 0 && type <= 3)) {
	    int len;
	    if (type >= 100 && type < 150) {
		len = type - 100;
	    } else {
		len = (int) bytesToLong(reader.getBytes(type + 1));
	    }
	    String s = new String(reader.getBytes(len), "UTF-8");
	    strings.add(s);
	    return s;
	} else if ((type >= 150 && type < 200) || (type >= 4 && type <= 7)) {
	    int idx;
	    if (type >= 150 && type < 200) {
		idx = type - 150;
	    } else {
		idx = (int) bytesToLong(reader.getBytes(type - 3));
	    }
	    return strings.get(idx);
	} else if (type == 19) {
	    return true;
	} else if (type == 18) {
	    return false;
	} else if (type == 16) {
	    Hashtable<String, Object> hash = new Hashtable<String, Object>();
	    while (reader.peekByte() != 255) {
		String key = (String) readObject(reader, strings);
		Object val = readObject(reader, strings);
		hash.put(key, val);
	    }
	    reader.getByte();
	    return hash;
	} else if (type == 17) {
	    int length = 0;
	    int alloced = 64;
	    Object[] arr = new Object[alloced];
	    while (reader.peekByte() != 255) {
		Object o = readObject(reader, strings);
		if (length == alloced) {
		    Object[] newarr = new Object[alloced * 2];
		    System.arraycopy(arr, 0, newarr, 0, alloced);
		    alloced *= 2;
		    arr = newarr;
		}
		arr[length++] = o;
	    }
	    reader.getByte();
	    if (length != alloced) {
		Object[] newarr = new Object[length];
		System.arraycopy(arr, 0, newarr, 0, length);
		alloced *= 2;
		arr = newarr;
	    }
	    return arr;
	} else if (type == 20) {
	    datalen = bytesToLong(reader.getBytes(8));
	    return datalen;
	}
	throw (new InputMismatchException());
    }

    public Object getResult() throws IOException, InputMismatchException {
	PCloudAPIReader reader = new PCloudAPIReader(sock.getInputStream());
	PCloudAPIStringCollection strings = new PCloudAPIStringCollection();
	return readObject(reader, strings);
    }

    public Object sendCommand(String method, Map<String, Object> params, boolean hasdata, long datalen) throws UnsupportedEncodingException, IOException, InputMismatchException {
	byte[] data = new byte[2048];
	byte[] methodb = method.getBytes("UTF-8");
	int off = 2;
	int cmdlen = methodb.length;
	int paramcnt = params.size();
	if (hasdata) {
	    byte[] arr = longToByte(datalen);
	    data[off++] = (byte) (cmdlen + 0x80);
	    for (int i = 0; i < 8; i++) {
		data[off++] = arr[i];
	    }
	} else {
	    data[off++] = (byte) cmdlen;
	}
	for (int i = 0; i < cmdlen; i++) {
	    data[off++] = methodb[i];
	}
	data[off++] = (byte) paramcnt;
	Iterator it = params.entrySet().iterator();
	while (it.hasNext()) {
	    Map.Entry pair = (Map.Entry) it.next();
	    byte[] name = ((String) pair.getKey()).getBytes("UTF-8");
	    Object value = pair.getValue();
	    int namelen = name.length;
	    if (value.getClass() == String.class) {
		String s = (String) value;
		byte[] b = s.getBytes("UTF-8");
		data[off++] = (byte) (namelen);
		for (int i = 0; i < namelen; i++) {
		    data[off++] = name[i];
		}
		long slen = b.length;
		byte[] blen = longToByte(slen);
		for (int i = 0; i < 4; i++) {
		    data[off++] = blen[i];
		}
		for (int i = 0; i < slen; i++) {
		    data[off++] = b[i];
		}
	    } else if (value.getClass() == Boolean.class) {
		Boolean b = (Boolean) value;
		data[off++] = (byte) (namelen + 0x80);
		for (int i = 0; i < namelen; i++) {
		    data[off++] = name[i];
		}
		if (b) {
		    data[off++] = 1;
		} else {
		    data[off++] = 0;
		}
	    } else if (value.getClass() == Long.class || value.getClass() == Integer.class) {
		long l;
		if (value.getClass() == Integer.class) {
		    l = (Integer) value;
		} else {
		    l = (Long) value;
		}
		data[off++] = (byte) (namelen + 0x40);
		for (int i = 0; i < namelen; i++) {
		    data[off++] = name[i];
		}
		byte[] arr = longToByte(l);
		for (int i = 0; i < 8; i++) {
		    data[off++] = arr[i];
		}
	    }
	}
	data[0] = (byte) ((off - 2) % 256);
	data[1] = (byte) ((off - 2) / 256);
	sock.getOutputStream().write(data, 0, off);
	if (hasdata) {
	    return true;
	} else {
	    return getResult();
	}
    }

    public Object sendCommand(String method) throws UnsupportedEncodingException, IOException {
	Map<String, Object> empty = new HashMap<String, Object>();
	return sendCommand(method, empty, false, 0);
    }

    public Object sendCommand(String method, Map<String, Object> params) throws UnsupportedEncodingException, IOException {
	return sendCommand(method, params, false, 0);
    }

    public Object sendCommand(String method, Map<String, Object> params, InputStream istream) throws UnsupportedEncodingException, IOException {
	long size = istream.available();
	long off = 0;
	int rb;
	OutputStream ostream = sock.getOutputStream();
	sendCommand(method, params, true, size);
	byte[] buff = new byte[4096];
	while (off < size) {
	    if (size - off > 4096) {
		rb = 4096;
	    } else {
		rb = (int) (size - off);
	    }
	    rb = istream.read(buff, 0, rb);
	    if (rb == -1) {
		throw (new IOException());
	    }
	    ostream.write(buff, 0, rb);
	    off += rb;
	}
	return getResult();
    }

    public void writeData(byte[] b, int off, int len) throws IOException {
	sock.getOutputStream().write(b, off, len);
    }

    public int readData(byte[] b, int off, int len) throws IOException {
	if (datalen == 0) {
	    return -1;
	}
	if ((long) len > datalen) {
	    len = (int) datalen;
	}
	int ret = sock.getInputStream().read(b, off, len);
	if (ret == -1) {
	    return -1;
	}
	datalen -= ret;
	return ret;
    }

    public void readData(OutputStream ostream) throws IOException {
	InputStream istream = sock.getInputStream();
	byte[] buff = new byte[4096];
	int rb;
	while (datalen > 0) {
	    if (datalen > 4096) {
		rb = 4096;
	    } else {
		rb = (int) datalen;
	    }
	    rb = istream.read(buff, 0, rb);
	    if (rb == -1) {
		break;
	    }
	    datalen -= rb;
	    ostream.write(buff, 0, rb);
	}
    }

    /**
     * Returns long value of the key in the obj collection.
     * 
     * @param obj the Hashtable Object containing the server response
     * @param key the key to get the long value from obj
     * @return The long value in this collection. Null if obj is null or no such key.
     */
    public static Long resultOptLong(Object obj, String key) {
	if (obj == null) {
	    return null;
	}
	if (obj.getClass() == Hashtable.class) {

	    Hashtable hash = (Hashtable) obj;
	    if (!hash.containsKey(key)) {
		return null;
	    }
	    Object o = hash.get(key);
	    if (o.getClass() == Long.class) {
		return (Long) o;
	    }
	}
	return null;
    }

    /**
     * Returns long value of the key in the obj collection.
     * @param obj the Hashtable Object containing the server response
     * @param key the key to get the long value from obj
     * @return The long value in this collection. Beware of NullPointerException if no such key. Better use resultOptLong
     * @throws InputMismatchException
     */
    public static long resultGetLong(Object obj, String key) throws InputMismatchException {
	if (obj.getClass() == Hashtable.class) {
	    Hashtable hash = (Hashtable) obj;
	    Object o = hash.get(key);
	    if (o.getClass() == Long.class) {
		return (Long) o;
	    }
	}
	throw (new InputMismatchException());
    }

    public static String resultOptString(Object obj, String key) throws InputMismatchException {
	if (obj == null) {
	    return null;
	}
	if (obj.getClass() == Hashtable.class) {
	    Hashtable hash = (Hashtable) obj;
	    if (!hash.containsKey(key)) {
		return null;
	    }
	    Object o = hash.get(key);
	    if (o.getClass() == String.class) {
		return (String) o;
	    }
	}
	throw (new InputMismatchException());
    }

    public static String resultGetString(Object obj, String key) throws InputMismatchException {
	if (obj.getClass() == Hashtable.class) {
	    Hashtable hash = (Hashtable) obj;
	    Object o = hash.get(key);
	    if (o.getClass() == String.class) {
		return (String) o;
	    }
	}
	throw (new InputMismatchException());
    }

    public static Boolean resultOptBoolean(Object obj, String key) throws InputMismatchException {
	if (obj == null) {
	    return null;
	}
	if (obj.getClass() == Hashtable.class) {
	    Hashtable hash = (Hashtable) obj;
	    if (!hash.containsKey(key)) {
		return null;
	    }
	    Object o = hash.get(key);
	    if (o.getClass() == Boolean.class) {
		return (Boolean) o;
	    }
	}
	throw (new InputMismatchException());
    }

    public static boolean resultGetBoolean(Object obj, String key) throws InputMismatchException {
	if (obj.getClass() == Hashtable.class) {
	    Hashtable hash = (Hashtable) obj;
	    Object o = hash.get(key);
	    if (o.getClass() == Boolean.class) {
		return (Boolean) o;
	    }
	}
	throw (new InputMismatchException());
    }

    public static Object[] resultOptArray(Object obj, String key) throws InputMismatchException {
	if (obj == null) {
	    return null;
	}
	if (obj.getClass() == Hashtable.class) {
	    Hashtable hash = (Hashtable) obj;
	    if (!hash.containsKey(key)) {
		return null;
	    }
	    Object o = hash.get(key);
	    if (o.getClass() == Object[].class) {
		return (Object[]) o;
	    }
	}
	throw (new InputMismatchException());
    }
    
    public static Object[] resultGetArray(Object obj, String key) throws InputMismatchException {
	if (obj.getClass() == Hashtable.class) {
	    Hashtable hash = (Hashtable) obj;
	    Object o = hash.get(key);
	    if (o.getClass() == Object[].class) {
		return (Object[]) o;
	    }
	}
	throw (new InputMismatchException());
    }

    public static Hashtable resultOptHashtable(Object obj, String key) throws InputMismatchException {
	if (obj == null) {
	    return null;
	}
	if (obj.getClass() == Hashtable.class) {
	    Hashtable hash = (Hashtable) obj;
	     if (!hash.containsKey(key)) {
		return null;
	    }
	    Object o = hash.get(key);
	    if (o.getClass() == Hashtable.class) {
		return (Hashtable) o;
	    }
	}
	throw (new InputMismatchException());
    }
    
    public static Hashtable resultGetHashtable(Object obj, String key) throws InputMismatchException {
	if (obj.getClass() == Hashtable.class) {
	    Hashtable hash = (Hashtable) obj;
	    Object o = hash.get(key);
	    if (o.getClass() == Hashtable.class) {
		return (Hashtable) o;
	    }
	}
	throw (new InputMismatchException());
    }
}
