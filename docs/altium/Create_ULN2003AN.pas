{******************************************************************************
  Altium Designer Script — ساخت سمبل ULN2003AN (DIP-16) برای گلدون هوشمند

  اجرا:
    1) File → New → Library → Schematic Library  (یا SchLib باز)
    2) DXF → Run Script → این فایل را انتخاب کن
       یا: File → Run Script → Create_ULN2003AN.pas
    3) بعد از ساخت، Save SchLib
    4) در SchDoc شیت ۴: Place → Part → همین کتابخانه → U3

  پین‌های استفاده‌شده پروژه:
    1B(1)←GPIO10   2B(2)←GPIO11
    1C(16)→Relay1.IN   2C(15)→Relay2.IN
    GND(8)→GND   COM(9)→+5V_BANK
******************************************************************************}

Procedure AddPin(Comp : ISch_Component; Des : String; Nam : String;
                 Elec : TPinElectrical; X, Y : Integer; Orient : TRotationBy90);
Var
  Pin : ISch_Pin;
Begin
  Pin := SchServer.SchObjectFactory(ePin, eCreate_Default);
  If Pin = Nil Then Exit;
  Pin.Location := Point(X, Y);
  Pin.PinLength := 200;
  Pin.Name := Nam;
  Pin.Designator := Des;
  Pin.Electrical := Elec;
  Pin.Orientation := Orient;
  Pin.Name_PositionLocked := True;
  Pin.Designator_PositionLocked := True;
  Comp.AddSchObject(Pin);
End;

Procedure Create_ULN2003AN;
Var
  Doc : IServerDocument;
  CurrentLib : ISch_Lib;
  Comp : ISch_Component;
  Rect : ISch_Rectangle;
  Lab : ISch_Label;
Begin
  // اگر SchLib باز نیست، یکی بساز
  If SchServer = Nil Then
  Begin
    ShowMessage('SchServer در دسترس نیست. اول یک Schematic Library باز کن.');
    Exit;
  End;

  CurrentLib := SchServer.GetCurrentSchDocument;
  If (CurrentLib = Nil) Or (CurrentLib.ObjectID <> eSchLib) Then
  Begin
    ShowMessage('لطفاً اول File → New → Library → Schematic Library را باز کن، بعد اسکریپت را اجرا کن.');
    Exit;
  End;

  Comp := SchServer.SchObjectFactory(eSchComponent, eCreate_Default);
  If Comp = Nil Then Exit;

  Comp.CurrentPartID := 1;
  Comp.DisplayMode := 0;
  Comp.LibReference := 'ULN2003AN';
  Comp.Designator.Text := 'U?';
  Comp.Designator.Location := Point(0, 1200);
  Comp.Comment.Text := 'ULN2003AN';
  Comp.Comment.Location := Point(0, -1200);
  Comp.ComponentDescription := '7-channel Darlington array — Goldon Hoshmand U3';

  // بدنه سمبل
  Rect := SchServer.SchObjectFactory(eRectangle, eCreate_Default);
  Rect.Location := Point(-600, -900);
  Rect.Corner := Point(600, 900);
  Rect.LineWidth := eSmall;
  Rect.Color := $000080;
  Rect.AreaColor := $FFFFFF;
  Rect.IsSolid := False;
  Comp.AddSchObject(Rect);

  Lab := SchServer.SchObjectFactory(eLabel, eCreate_Default);
  Lab.Location := Point(-400, 0);
  Lab.Text := 'ULN2003AN';
  Lab.FontID := 1;
  Comp.AddSchObject(Lab);

  // سمت چپ — ورودی‌ها (ناچ بالا: پین ۱ بالا)
  // Y از بالا به پایین: 1..8
  AddPin(Comp, '1',  '1B',  eElectricInput,         -800,  700, eRotate180);
  AddPin(Comp, '2',  '2B',  eElectricInput,         -800,  500, eRotate180);
  AddPin(Comp, '3',  '3B',  eElectricInput,         -800,  300, eRotate180);
  AddPin(Comp, '4',  '4B',  eElectricInput,         -800,  100, eRotate180);
  AddPin(Comp, '5',  '5B',  eElectricInput,         -800, -100, eRotate180);
  AddPin(Comp, '6',  '6B',  eElectricInput,         -800, -300, eRotate180);
  AddPin(Comp, '7',  '7B',  eElectricInput,         -800, -500, eRotate180);
  AddPin(Comp, '8',  'GND', eElectricPower,         -800, -700, eRotate180);

  // سمت راست — خروجی‌ها + COM (پین ۱۶ بالا، ۹ پایین)
  AddPin(Comp, '16', '1C',  eElectricOpenCollector,  800,  700, eRotate0);
  AddPin(Comp, '15', '2C',  eElectricOpenCollector,  800,  500, eRotate0);
  AddPin(Comp, '14', '3C',  eElectricOpenCollector,  800,  300, eRotate0);
  AddPin(Comp, '13', '4C',  eElectricOpenCollector,  800,  100, eRotate0);
  AddPin(Comp, '12', '5C',  eElectricOpenCollector,  800, -100, eRotate0);
  AddPin(Comp, '11', '6C',  eElectricOpenCollector,  800, -300, eRotate0);
  AddPin(Comp, '10', '7C',  eElectricOpenCollector,  800, -500, eRotate0);
  AddPin(Comp, '9',  'COM', eElectricPower,          800, -700, eRotate0);

  CurrentLib.AddSchComponent(Comp);
  CurrentLib.CurrentSchComponent := Comp;
  CurrentLib.GraphicallyInvalidate;

  ShowMessage(
    'سمبل ULN2003AN ساخته شد.' + #13#10 +
    '۱) SchLib را Save کن' + #13#10 +
    '۲) در شیت ۴: Place → Part → U3' + #13#10 +
    '۳) نت‌ها را مطابق Sheet4-ULN2003-guide.txt وصل کن'
  );
End;
