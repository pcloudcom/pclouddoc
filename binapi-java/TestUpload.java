import org.pcloud.*;
import java.util.*;
import java.io.*;

public class TestUpload{
  public static void main(String args[]){
    try{
      if (args.length==0){
        System.out.println("Please specify filename as parameter");
        return;
      }
      File file=new File(args[0]);
      FileInputStream ifile=new FileInputStream(file);
      PCloudAPI conn=new PCloudAPI(true);
      Hashtable <String, Object> params=new Hashtable <String, Object> ();
      params.put("auth", "Ec7QkEjFUnzZ7Z8W2YH1qLgxY7gGvTe09AH0i7V3kX");
      params.put("folderid", 0);
      params.put("filename", args[0]);
      PCloudAPIDebug.print(conn.sendCommand("uploadfile", params, ifile));
    }
    catch(Exception e){
      e.printStackTrace();
    }
  }
}
