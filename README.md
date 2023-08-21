# QQDrawPanel
## 瞎几把写的QQ群画板bot



# 指令表

0xFFFFFFFF

ARGB(A,R,G,B)

.Pen(clr,cxPen)

.Brush(clr)

.Brush(x1,y1,x2,y2,clr1,clr2)

.Font(Text,size,style)

.ListPen(clrbk)     lParam=QDPLISTOBJ*

.ListBrush(clrbk)     lParam=QDPLISTOBJ*

.ListFont()      lParam=QDPLISTOBJ*

.DeletePen(id)

.DeleteBrush(id)

.DeleteFont(id)

.SelPen(id)

.SelBrush(id)

.SelFont(id)

.Clear(clr)

.Clear()

.DrawPixel(x,y)

.DrawLine(x1,y1,x2,y2)

.DrawPolyLine({x1,y1,x2,y2,...})

.DrawRect(x1,y1,x2,y2)

.DrawArc(x1,y1,x2,y2,angleStart,angleSweep)

.DrawEllipse(x1,y1,x2,y2)

.DrawPie(x1,y1,x2,y2,angleStart,angleSweep)

.DrawImage(xDst,yDst) Image    lParam=QDPDRAWIMAGE*

.DrawImage(xDst,yDst,cxDst,cyDst) Image    lParam=QDPDRAWIMAGE*

.DrawImage(xDst,yDst,cxDst,cyDst,xSrc,ySrc,cxSrc,cySrc) Image    lParam=QDPDRAWIMAGE*

.FillText(x,y,cx,cy,wrap,alignH,alignV) Text    lParam=pszText

.SetTransform({m11,m12,m21,m22,dx,dy})

.MultiplyTransform({m11,m12,m21,m22,dx,dy},order)

.MatrixTranslate(x,y)    lParam=QDPBIN*

.MatrixShear(s,t)    lParam=QDPBIN*

.MatrixShear(x,y,s,t)    lParam=QDPBIN*

.MatrixRotate(angle)    lParam=QDPBIN*

.MatrixRotate(x,y,angle)    lParam=QDPBIN*

.MatrixScale(s,t)    lParam=QDPBIN*

.MatrixScale(x,y,s,t)    lParam=QDPBIN*

.MatrixInvert({m11,m12,m21,m22,dx,dy})    lParam=QDBIN*

.ReSize(cx,cy)

.GetContent()

.GetImageSize() Image    lParam=QDPGETIMAGESIZE*

.GetStringSize(x,y,cx,cy,wrap,alignH,alignV) Text    lParam=QDPGETSTRINGSIZE*

.GetObjCount(type)