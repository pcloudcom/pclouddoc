import org.pcloud.*;
import java.util.*;

public class TestListRoot{
  public static void main(String args[]){
    try{
      PCloudAPI conn=new PCloudAPI();
      Hashtable <String, Object> params=new Hashtable <String, Object> ();
      params.put("auth", "Ec7QkEjFUnzZ7Z8W2YH1qLgxY7gGvTe09AH0i7V3kX");
      params.put("folderid", 0);
      Object res=conn.sendCommand("listfolder", params);
      if (PCloudAPI.resultGetLong(res, "result")!=0){
        System.out.println("Error: "+PCloudAPI.resultGetString(res, "error"));
        return;
      }
      Object[] contents=PCloudAPI.resultGetArray(PCloudAPI.resultGetHashtable(res, "metadata"), "contents");
      for (int i=0; i<contents.length; i++){
        Object entry=contents[i];
        if (PCloudAPI.resultGetBoolean(entry, "isfolder"))
           System.out.println("Folder: "+PCloudAPI.resultGetString(entry, "name"));
        else
           System.out.println("File: "+PCloudAPI.resultGetString(entry, "name")+" Size: "+PCloudAPI.resultGetLong(entry, "size"));
      }
    }
    catch(Exception e){
      e.printStackTrace();
    }
  }
}
