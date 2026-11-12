"""Readable, vector-based PDF report. No changes to recorded data or scoring."""
from datetime import datetime
from pathlib import Path
from xml.sax.saxutils import escape

from reportlab.lib import colors
from reportlab.lib.pagesizes import A4
from reportlab.lib.styles import ParagraphStyle
from reportlab.pdfgen import canvas
from reportlab.platypus import Paragraph, Table, TableStyle

from evaluation import EXERCISES, evaluate, progression, validate_criteria
from presentation import result_label, short_feedback

INK=colors.HexColor('#17283d')
BLUE=colors.HexColor('#176eaa')
MUTED=colors.HexColor('#526578')
LIGHT=colors.HexColor('#edf3f8')
GREEN=colors.HexColor('#16734a')
ORANGE=colors.HexColor('#a84317')
WIDTH,HEIGHT=A4
LEFT=40
CONTENT=WIDTH-80
STYLE=ParagraphStyle('body',fontName='Helvetica',fontSize=9,leading=12,textColor=INK)
SMALL=ParagraphStyle('small',parent=STYLE,fontSize=8,leading=10)


def paragraph(c, text, x, top, width, style=STYLE):
    p=Paragraph(escape(text).replace('\n','<br/>'),style)
    _,h=p.wrap(width,HEIGHT)
    p.drawOn(c,x,top-h)
    return top-h


def chart(c, x, top, width, height, values, labels, goal, title, percent=False):
    c.setFillColor(INK);c.setFont('Helvetica-Bold',10);c.drawString(x,top,title)
    left=x+31;right=x+width-8;bottom=top-height+25;upper=top-20
    maximum=100 if percent else max([goal]+[v for v in values if v is not None]+[1])*1.15
    for fraction in (0,.5,1):
        y=bottom+(upper-bottom)*fraction
        c.setStrokeColor(colors.HexColor('#dce5ee'));c.line(left,y,right,y)
        c.setFillColor(MUTED);c.setFont('Helvetica',7)
        c.drawRightString(left-5,y-2,f'{maximum*fraction:.0f}')
    y=bottom+(upper-bottom)*goal/maximum
    c.setDash(4,3);c.setStrokeColor(ORANGE);c.line(left,y,right,y);c.setDash()
    # Sample labels for long histories; retain every value in the line plot.
    stride=max(1,(len(labels)+7)//8)
    previous=None
    for i,(value,label) in enumerate(zip(values,labels)):
        px=left+(right-left)*(i/(len(labels)-1) if len(labels)>1 else .5)
        if i%stride==0 or i==len(labels)-1:
            c.setFillColor(MUTED);c.setFont('Helvetica',7);c.drawCentredString(px,bottom-12,str(label))
        if value is None:
            previous=None;continue
        py=bottom+(upper-bottom)*value/maximum
        c.setStrokeColor(BLUE);c.setFillColor(BLUE)
        if previous:c.line(*previous,px,py)
        c.circle(px,py,2,stroke=0,fill=1)
        previous=(px,py)
    c.setFont('Helvetica',7);c.setFillColor(MUTED)
    c.drawString(x,top-height+3,f'Attempt number  |  dashed goal: {goal:g}' + ('%' if percent else 's'))


def _header(c,participant,page,total,exercise,band):
    c.setFillColor(INK);c.setFont('Helvetica-Bold',22);c.drawString(LEFT,HEIGHT-48,'Exercise progress report')
    paragraph(c,f'Participant: {participant}',LEFT,HEIGHT-62,CONTENT)
    c.setFont('Helvetica-Bold',15);c.drawString(LEFT,HEIGHT-105,f'{EXERCISES[exercise]}  |  target zone +/-{band:g} cm')
    c.setFillColor(MUTED);c.setFont('Helvetica',8)
    c.drawString(LEFT,25,'ECE198 | Distance and duration measures; not a full-body form assessment.')
    c.drawRightString(WIDTH-LEFT,25,f'{page} / {total}')


def _table(c,top,entries,targets,minimum):
    data=[['Attempt','Held / goal','Consistency','Result and reason']]
    for index,row in entries:
        r=evaluate(row,targets,minimum)
        pct='Unavailable' if r['consistency_percent'] is None else f"{r['consistency_percent']:.1f}%"
        text=result_label(r,minimum)+'\n'+short_feedback(r,minimum)
        data.append([str(index+1),f"{r['in_band_seconds']:.1f}s / {r['target_seconds']:g}s",pct,Paragraph(escape(text).replace('\n','<br/>'),SMALL)])
    table=Table(data,colWidths=[48,88,82,CONTENT-218])
    table.setStyle(TableStyle([
        ('BACKGROUND',(0,0),(-1,0),INK),('TEXTCOLOR',(0,0),(-1,0),colors.white),
        ('FONTNAME',(0,0),(-1,0),'Helvetica-Bold'),('FONTNAME',(0,1),(-1,-1),'Helvetica'),
        ('FONTSIZE',(0,0),(-1,-1),8),('VALIGN',(0,0),(-1,-1),'TOP'),
        ('TOPPADDING',(0,0),(-1,-1),7),('BOTTOMPADDING',(0,0),(-1,-1),7),
        ('ROWBACKGROUNDS',(0,1),(-1,-1),[LIGHT,colors.white]),
        ('LINEBELOW',(0,0),(-1,0),.5,INK)]))
    _,h=table.wrap(CONTENT,HEIGHT)
    table.drawOn(c,LEFT,top-h)
    return top-h


def build_report(path, participant, rows, targets, minimum):
    """Write a new PDF, grouped by exercise and tolerance; refuse overwrite."""
    validate_criteria(targets,minimum)
    if not participant.strip() or not rows:raise ValueError('Choose a participant with saved attempts first.')
    groups={}
    for index,row in enumerate(rows):
        # Same tolerance rule as progression(), without mixing incompatible records.
        group=next((key for key in groups if key[0]==row['exercise'] and abs(key[1]-row['band_cm'])<.001),None)
        if group is None:group=(row['exercise'],row['band_cm']);groups[group]=[]
        groups[group].append((index,row))
    # Summary + graphs have their own page. Tables continue on separate pages,
    # keeping reports readable even with hundreds of attempts.
    total=sum(1+(len(entries)+7)//8 for entries in groups.values())
    path=Path(path)
    with path.open('xb') as output:
        c=canvas.Canvas(output,pagesize=A4)
        c.setTitle(f'Exercise progress - {participant}')
        c.setAuthor('ECE198 Hold Results')
        page=0
        for (exercise,band),entries in groups.items():
            page+=1;_header(c,participant,page,total,exercise,band)
            top=paragraph(c,f'Good requires BOTH: at least {targets[exercise]:g}s in the target zone AND {minimum:g}% consistency.',LEFT,HEIGHT-120,CONTENT)
            index,row=entries[-1];r=evaluate(row,targets,minimum)
            top-=23;c.setFillColor(INK);c.setFont('Helvetica-Bold',12)
            c.drawString(LEFT,top,f'Latest {EXERCISES[exercise].lower()} result - Attempt {index+1}')
            c.setFillColor(GREEN if r['status']=='Good' else ORANGE if r['status']=='Not good yet' else MUTED)
            c.setFont('Helvetica-Bold',13);c.drawString(LEFT,top-22,result_label(r,minimum))
            top=paragraph(c,short_feedback(r,minimum),LEFT,top-32,CONTENT)
            pct='Unavailable' if r['consistency_percent'] is None else f"{r['consistency_percent']:.1f}%"
            assessed='Unavailable' if r['adjusted_active_seconds'] is None else f"{r['adjusted_active_seconds']:.1f}s"
            excluded='Unavailable' if r['trailing_seconds'] is None else f"{r['trailing_seconds']:.1f}s"
            top-=15
            cards=[('Time in target zone',f"{r['in_band_seconds']:.1f}s / {targets[exercise]:g}s"),('Consistency',f'{pct} / {minimum:g}%'),('Assessed / active',f"{assessed} / {r['active_seconds']:.1f}s"),('Ending excluded',excluded)]
            for i,(label,value) in enumerate(cards):
                x=LEFT+i*(CONTENT/4)
                c.setFillColor(LIGHT);c.roundRect(x,top-57,CONTENT/4-6,57,5,stroke=0,fill=1)
                c.setFillColor(MUTED);c.setFont('Helvetica',8);c.drawString(x+8,top-16,label)
                c.setFillColor(INK);c.setFont('Helvetica-Bold',11);c.drawString(x+8,top-37,value)
            top-=83
            change=progression(rows,index,targets,minimum)
            if change:
                text=f"Since previous {EXERCISES[exercise].lower()} attempt {change['previous_index']+1}: held time {change['hold_change']:+.1f}s; consistency {change['consistency_change']:+.1f} percentage points.\nSince first comparable attempt: held time {change['first_hold_change']:+.1f}s; consistency {change['first_consistency_change']:+.1f} percentage points."
            else:text='No earlier scored attempt with this exercise and tolerance is available for comparison.'
            top=paragraph(c,text,LEFT,top,CONTENT)
            scored=[evaluate(item,targets,minimum) for _,item in entries]
            top-=30;half=(CONTENT-24)/2
            chart(c,LEFT,top,half,185,[v['in_band_seconds'] for v in scored],[i+1 for i,_ in entries],targets[exercise],'Time in target zone (s)')
            chart(c,LEFT+half+24,top,half,185,[v['consistency_percent'] for v in scored],[i+1 for i,_ in entries],minimum,'Consistency (%)',True)
            top-=215
            notes=('How to read this report\n'
                   'Consistency = time in the target zone / assessed duration x 100. Assessed duration excludes pauses and the final continuous departure from the zone. Earlier departures followed by a return still count against consistency.\n'
                   'Missing echoes or slow sampling can reduce credited time. These measurements cannot assess full exercise form.\n'
                   'Attempt numbers follow saved history order, not calendar dates. All attempts are assessed with the current confirmed goals; graphs compare only the same exercise and tolerance.\n'
                   f'Report generated: {datetime.now().strftime("%Y-%m-%d %H:%M")} (computer time). Session dates were not recorded.')
            paragraph(c,notes,LEFT,top,CONTENT,SMALL)
            c.showPage()
            for start in range(0,len(entries),8):
                page+=1;_header(c,participant,page,total,exercise,band)
                top=paragraph(c,f'Attempt history | goal {targets[exercise]:g}s and consistency {minimum:g}%. Same confirmed rules for every attempt.',LEFT,HEIGHT-125,CONTENT)
                _table(c,top-15,entries[start:start+8],targets,minimum)
                paragraph(c,'Held time means recorded time inside the calibrated target zone. Unavailable scores are not treated as zero. See the summary page for graphs and definitions.',LEFT,80,CONTENT,SMALL)
                c.showPage()
        c.save()
    return path
