# QQDrawPanel
## 瞎几把写的QQ群画板bot



# 指令表

0xFFFFFFFF
ARGB(A,R,G,B)

.Pen(clr,cxPen)

.Brush(clr)

.Font(Text,size,style)

.ListPen()     lParam=QDPELISTPEN*

.ListBrush()     lParam=QDPELISTBRUSH*

.SelPen(id)

.SelBrush(id)

.Clear(clr)

.DrawPixel(x,y)

.DrawLine(x1,y1,x2,y2)

.DrawPolyLine({x1,y1,x2,y2,...})

.DrawRect(x1,y1,x2,y2)

.DrawArc(x1,y1,x2,y2,angleStart,angleSweep)

.DrawEllipse(x1,y1,x2,y2)

.DrawPie(x1,y1,x2,y2,angleStart,angleSweep)

.DrawImage(xDst,yDst) Image    lParam=QDPDRAWIMAGE*

.DrawImage(xDst,yDst,cxDst,cyDst) Image     lParam=QDPDRAWIMAGE*

.DrawImage(xDst,yDst,cxDst,cyDst,xSrc,ySrc,cxSrc,cySrc) Image    lParam=QDPDRAWIMAGE*

.DrawText(x,y,cx,cy,wrap,align) Text    lParam=pszText

.SetTransform({m11,m12,m21,m22,dx,dy})

.MultiplyTransform({m11,m12,m21,m22,dx,dy},order)

.GetImageSize() Image    lParam=QDPGETIMAGESIZE*