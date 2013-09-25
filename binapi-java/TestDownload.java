import org.pcloud.*;
import java.util.*;
import java.io.*;

public class TestDownload{
  public static void main(String args[]){
    try{
      File file=new File("01 The Wedding.zip");
      FileOutputStream ofile=new FileOutputStream(file);
      PCloudAPI conn=new PCloudAPI(true);
      Hashtable <String, Object> params=new Hashtable <String, Object> ();
      params.put("auth", "Ec7QkEjFUnzZ7Z8W2YH1qLgxY7gGvTe09AH0i7V3kX");
      params.put("fileids", "61996");
      PCloudAPIDebug.print(conn.sendCommand("getzip", params));
      conn.readData(ofile);
    }
    catch(Exception e){
      e.printStackTrace();
    }
  }
}
