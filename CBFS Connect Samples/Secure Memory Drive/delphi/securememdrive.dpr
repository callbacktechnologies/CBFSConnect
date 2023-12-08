(*
 * CBFS Connect 2024 Delphi Edition - Sample Project
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
 *)

program securememdrive;

uses
  Forms,
  virtfile in 'virtfile.pas' ,
  securememdrivef in 'securememdrivef.pas' {FormSecurememdrive};

begin
  Application.Initialize;

  Application.CreateForm(TFormSecurememdrive, FormSecurememdrive);


  Application.Run;
end.


         
