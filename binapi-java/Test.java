import org.pcloud.*;
import java.util.*;

public class Test{
  public static void main(String args[]){
    try{
      PCloudAPI conn=new PCloudAPI(true);
      Hashtable <String, Object> params=new Hashtable <String, Object> ();
      params.put("auth", "Ec7QkEjFUnzZ7Z8W2YH1qLgxY7gGvTe09AH0i7V3kX");
      params.put("folderid", 0);
//      conn.sendCommand("diff", params);
      PCloudAPIDebug.print(conn.sendCommand("diff", params));
    }
    catch(Exception e){
      e.printStackTrace();
    }
  }
}
