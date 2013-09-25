package org.pcloud;

import java.util.*;

public class PCloudAPIDebug{
  private static void print_ident(int cnt){
    while (cnt>0){
      cnt--;
      System.out.print("\t");
    }
  }

  private static void do_print(Object obj, int ident){
    if (obj.getClass()==String.class){
      System.out.print("\"");
      System.out.print(obj);
      System.out.print("\"");
    }
    else if (obj.getClass()==Long.class)
      System.out.print((Long)obj);
    else if (obj.getClass()==Boolean.class){
      if ((Boolean)obj)
        System.out.print("true");
      else
        System.out.print("false");
    }
    else if (obj.getClass()==Hashtable.class){
      Iterator it = ((Map)obj).entrySet().iterator();
      boolean first=true;
      System.out.println("{");
      while (it.hasNext()){
        Map.Entry pair=(Map.Entry)it.next();
        if (first)
          first=false;
        else
          System.out.println(",");
        print_ident(ident+1);
        do_print(pair.getKey(), ident+1);
        System.out.print("=");
        do_print(pair.getValue(), ident+1);
      }
      System.out.println("");
      print_ident(ident);
      System.out.print("}");
    }
    else if (obj.getClass()==Object[].class){
      Object[] arr=(Object[])obj;
      System.out.println("[");
      for (int i=0; i<arr.length; i++){
        if (i>0)
          System.out.println(",");
        print_ident(ident+1);
        do_print(arr[i], ident+1);
      }
      System.out.println("");
      print_ident(ident);
      System.out.print("]");
    }
  }
  
  public static void print(Object obj){
    do_print(obj, 0);
  }
}
