"""Readable, vector-based PDF report. No changes to recorded data or scoring."""
from pathlib import Path
from xml.sax.saxutils import escape

import reportlab
from reportlab.lib import colors
from reportlab.lib.pagesizes import A4
from reportlab.lib.styles import ParagraphStyle
from reportlab.pdfgen import canvas
from reportlab.pdfbase import pdfmetrics
from reportlab.pdfbase.ttfonts import TTFont
from reportlab.platypus import Paragraph, Table, TableStyle

from evaluation import EXERCISES, COUNT_EXERCISES, evaluate, progression, validate_criteria, attempt_number
from presentation import result_label, short_feedback, progress_summary

# Embed a consistent font so the report looks the same on another computer.
_FONT_DIR=Path(reportlab.__file__).parent/'fonts'
pdfmetrics.registerFont(TTFont('Report',str(_FONT_DIR/'Vera.ttf')))
pdfmetrics.registerFont(TTFont('Report-Bold',str(_FONT_DIR/'VeraBd.ttf')))

INK=colors.HexColor('#17283d')
BLUE=colors.HexColor('#176eaa')
MUTED=colors.HexColor('#526578')
LIGHT=colors.HexColor('#edf3f8')
GREEN=colors.HexColor('#16734a')
ORANGE=colors.HexColor('#a84317')
WIDTH,HEIGHT=A4
LEFT=40
CONTENT=WIDTH-80
STYLE=ParagraphStyle('body',fontName='Report',fontSize=9,leading=12,textColor=INK)
SMALL=ParagraphStyle('small',parent=STYLE,fontSize=8,leading=10)


def paragraph(c, text, x, top, width, style=STYLE):
    p=Paragraph(escape(text).replace('\n','<br/>'),style)
    _,h=p.wrap(width,HEIGHT)
    p.drawOn(c,x,top-h)
    return top-h


def chart(c, x, top, width, height, values, labels, goal, title, percent=False, unit="s"):
    c.setFillColor(INK);c.setFont('Report-Bold',10);c.drawString(x,top,title)
    left=x+31;right=x+width-8;bottom=top-height+25;upper=top-20
    maximum=100 if percent else max(([goal] if goal is not None else [])+[v for v in values if v is not None]+[1])*1.15
    for fraction in (0,.5,1):
        y=bottom+(upper-bottom)*fraction
        c.setStrokeColor(colors.HexColor('#dce5ee'));c.line(left,y,right,y)
        c.setFillColor(MUTED);c.setFont('Report',7)
        c.drawRightString(left-5,y-2,f'{maximum*fraction:.0f}')
    if goal is not None:
        y=bottom+(upper-bottom)*goal/maximum
        c.setDash(4,3);c.setStrokeColor(ORANGE);c.line(left,y,right,y);c.setDash()
    # Sample labels for long histories; retain every value in the line plot.
    stride=max(1,(len(labels)+7)//8)
    previous=None
    for i,(value,label) in enumerate(zip(values,labels)):
        px=left+(right-left)*(i/(len(labels)-1) if len(labels)>1 else .5)
        if i%stride==0 or i==len(labels)-1:
            c.setFillColor(MUTED);c.setFont('Report',7);c.drawCentredString(px,bottom-12,str(label))
        if value is None:
            previous=None;continue
        py=bottom+(upper-bottom)*value/maximum
        c.setStrokeColor(BLUE);c.setFillColor(BLUE)
        if previous:c.line(*previous,px,py)
        c.circle(px,py,2,stroke=0,fill=1)
        previous=(px,py)
    c.setFont('Report',7);c.setFillColor(MUTED)
    c.drawString(x,top-height+3,'Session number' + (f'  |  dashed goal: {goal:g}' + ('%' if percent else unit) if goal is not None else ''))


def _header(c,participant,page,total,exercise,band):
    c.setFillColor(INK);c.setFont('Report-Bold',22);c.drawString(LEFT,HEIGHT-48,'Exercise progress report')
    paragraph(c,f'Participant: {participant}',LEFT,HEIGHT-62,CONTENT)
    title=f'{EXERCISES[exercise]}  |  Part A recorded reps' if exercise in COUNT_EXERCISES else 'Chest press  |  80% press / 20% return' if exercise=='chest_press' else f'{EXERCISES[exercise]}  |  target zone +/-{band:g} cm'
    c.setFont('Report-Bold',15);c.drawString(LEFT,HEIGHT-105,title)
    c.setFillColor(MUTED);c.setFont('Report',8)
    if page==total:
        c.drawString(LEFT,55,'How to read: compare results with goals; +/- shows change; dashed graph lines mark goals.')
    c.drawString(LEFT,25,'ECE198 | Exercise results')
    c.drawRightString(WIDTH-LEFT,25,f'{page} / {total}')


def _table(c,top,entries,targets,minimum,rep_minimum=90,all_rows=None):
    chest=entries[0][1]['exercise']=='chest_press'
    counts=entries[0][1]['exercise'] in COUNT_EXERCISES
    data=[['Session','Reps / goal','Active','Mean BPM','Result and reason']] if counts else [['Session','Good / goal','Good %','Travel','Result and reason']] if chest else [['Session','Held / goal','Consistency','Result and reason']]
    for index,row in entries:
        r=evaluate(row,targets,minimum,rep_minimum)
        number=attempt_number(all_rows,index) if all_rows else index+1
        if counts:
            bpm='Unavailable' if r['bpm_avg'] is None else f"{r['bpm_avg']:.1f}"
            text=result_label(r,minimum)+'\n'+short_feedback(r,minimum)
            data.append([str(number),f"{r['rep_count']} / {r['target_reps']:g}",f"{r['active_seconds']:.1f}s",bpm,Paragraph(escape(text).replace('\n','<br/>'),SMALL)])
        elif chest:
            pct='Unavailable' if r['good_rep_percent'] is None else f"{r['good_rep_percent']:.1f}%"
            text=result_label(r,rep_minimum)+'\n'+short_feedback(r,rep_minimum)
            detail=f"{row['good_reps']} good / {row['attempts']} attempts; {r['short_attempts']} short; active {r['active_seconds']:.1f}s."
            data.append([str(number),f"{r['good_reps']} / {r['target_reps']:g}",pct,f"{r['calibrated_travel_cm']:.1f}cm",Paragraph(escape(text+'\n'+detail).replace('\n','<br/>'),SMALL)])
        else:
            pct='Unavailable' if r['consistency_percent'] is None else f"{r['consistency_percent']:.1f}%"
            text=result_label(r,minimum)+'\n'+short_feedback(r,minimum)
            data.append([str(number),f"{r['in_band_seconds']:.1f}s / {r['target_seconds']:g}s",pct,Paragraph(escape(text).replace('\n','<br/>'),SMALL)])
    table=Table(data,colWidths=[48,76,58,57,CONTENT-239] if chest or counts else [48,88,82,CONTENT-218])
    table.setStyle(TableStyle([
        ('BACKGROUND',(0,0),(-1,0),INK),('TEXTCOLOR',(0,0),(-1,0),colors.white),
        ('FONTNAME',(0,0),(-1,0),'Report-Bold'),('FONTNAME',(0,1),(-1,-1),'Report'),
        ('FONTSIZE',(0,0),(-1,-1),8),('VALIGN',(0,0),(-1,-1),'TOP'),
        ('TOPPADDING',(0,0),(-1,-1),7),('BOTTOMPADDING',(0,0),(-1,-1),7),
        ('ROWBACKGROUNDS',(0,1),(-1,-1),[LIGHT,colors.white]),
        ('LINEBELOW',(0,0),(-1,0),.5,INK)]))
    _,h=table.wrap(CONTENT,HEIGHT)
    table.drawOn(c,LEFT,top-h)
    return top-h


def build_report(path, participant, rows, targets, minimum,rep_minimum=90):
    """Write a new PDF, grouped by exercise and tolerance; refuse overwrite."""
    validate_criteria(targets,minimum,rep_minimum)
    if {r["exercise"] for r in rows}-targets.keys():raise ValueError("Set targets for all loaded exercises before exporting.")
    if not participant.strip() or not rows:raise ValueError('Choose a participant with saved attempts first.')
    groups={}
    for index,row in enumerate(rows):
        # Same tolerance rule as progression(), without mixing incompatible records.
        no_band=row['exercise']=='chest_press' or row['exercise'] in COUNT_EXERCISES
        group=next((key for key in groups if key[0]==row['exercise'] and (no_band or abs(key[1]-row['band_cm'])<.001)),None)
        if group is None:group=(row['exercise'],None if no_band else row['band_cm']);groups[group]=[]
        groups[group].append((index,row))
    # Summary + graphs have their own page. Tables continue on separate pages,
    # keeping reports readable even with hundreds of attempts.
    total=sum(1+(len(entries)+7)//8 for entries in groups.values())
    path=Path(path)
    with path.open('xb') as output:
        c=canvas.Canvas(output,pagesize=A4)
        c.setTitle(f'Exercise progress - {participant}')
        c.setAuthor('ECE198 Exercise Results')
        page=0
        for (exercise,band),entries in groups.items():
            page+=1;_header(c,participant,page,total,exercise,band)
            if exercise in COUNT_EXERCISES:
                _count_summary(c,rows,entries,targets,minimum,rep_minimum)
                c.showPage()
                for start in range(0,len(entries),8):
                    page+=1;_header(c,participant,page,total,exercise,band)
                    top=paragraph(c,f'{EXERCISES[exercise]} history | goal {targets[exercise]:g} reps.',LEFT,HEIGHT-125,CONTENT)
                    _table(c,top-15,entries[start:start+8],targets,minimum,rep_minimum,rows)
                    c.showPage()
                continue
            if exercise=='chest_press':
                _rep_summary(c,rows,entries,targets,minimum,rep_minimum)
                c.showPage()
                for start in range(0,len(entries),8):
                    page+=1;_header(c,participant,page,total,exercise,band)
                    top=paragraph(c,f'Chest-press history | goal {targets[exercise]:g} good reps and {rep_minimum:g}% good reps.',LEFT,HEIGHT-125,CONTENT)
                    _table(c,top-15,entries[start:start+8],targets,minimum,rep_minimum,rows)
                    c.showPage()
                continue
            top=paragraph(c,f'Good requires BOTH: at least {targets[exercise]:g}s in the target zone AND {minimum:g}% consistency.',LEFT,HEIGHT-120,CONTENT)
            index,row=entries[-1];r=evaluate(row,targets,minimum)
            top-=23;c.setFillColor(INK);c.setFont('Report-Bold',12)
            c.drawString(LEFT,top,f'Latest {EXERCISES[exercise].lower()} result - Session {attempt_number(rows,index)}')
            c.setFillColor(GREEN if r['status']=='Good' else ORANGE if r['status']=='Not good yet' else MUTED)
            c.setFont('Report-Bold',13);c.drawString(LEFT,top-22,result_label(r,minimum))
            top=paragraph(c,short_feedback(r,minimum),LEFT,top-32,CONTENT)
            pct='Unavailable' if r['consistency_percent'] is None else f"{r['consistency_percent']:.1f}%"
            assessed='Unavailable' if r['adjusted_active_seconds'] is None else f"{r['adjusted_active_seconds']:.1f}s"
            excluded='Unavailable' if r['trailing_seconds'] is None else f"{r['trailing_seconds']:.1f}s"
            top-=15
            cards=[('Time in target zone',f"{r['in_band_seconds']:.1f}s / {targets[exercise]:g}s"),('Consistency',f'{pct} / {minimum:g}%'),('Assessed / active',f"{assessed} / {r['active_seconds']:.1f}s"),('Ending excluded',excluded)]
            for i,(label,value) in enumerate(cards):
                x=LEFT+i*(CONTENT/4)
                c.setFillColor(LIGHT);c.roundRect(x,top-57,CONTENT/4-6,57,5,stroke=0,fill=1)
                c.setFillColor(MUTED);c.setFont('Report',8);c.drawString(x+8,top-16,label)
                c.setFillColor(INK)
                size=11
                while pdfmetrics.stringWidth(value,'Report-Bold',size)>CONTENT/4-22:size-=.5
                c.setFont('Report-Bold',size);c.drawString(x+8,top-37,value)
            top-=83
            change=progression(rows,index,targets,minimum)
            if change:
                text=progress_summary(rows,index,change)+'\n'+progress_summary(rows,index,change,True)
            else:text='No earlier scored attempt with this exercise and tolerance is available for comparison.'
            top=paragraph(c,text,LEFT,top,CONTENT)
            scored=[evaluate(item,targets,minimum) for _,item in entries]
            top-=30;half=(CONTENT-24)/2
            chart(c,LEFT,top,half,185,[v['in_band_seconds'] for v in scored],[attempt_number(rows,i) for i,_ in entries],targets[exercise],'Time in target zone (s)')
            chart(c,LEFT+half+24,top,half,185,[v['consistency_percent'] for v in scored],[attempt_number(rows,i) for i,_ in entries],minimum,'Consistency (%)',True)
            c.showPage()
            for start in range(0,len(entries),8):
                page+=1;_header(c,participant,page,total,exercise,band)
                top=paragraph(c,f'Attempt history | goal {targets[exercise]:g}s and consistency {minimum:g}%.',LEFT,HEIGHT-125,CONTENT)
                _table(c,top-15,entries[start:start+8],targets,minimum,rep_minimum,rows)
                c.showPage()
        c.save()
    return path


def _count_summary(c,rows,entries,targets,minimum,rep_minimum):
    index,row=entries[-1];r=evaluate(row,targets,minimum,rep_minimum)
    exercise=row['exercise']
    top=paragraph(c,f'Goal: at least {targets[exercise]:g} reps.',LEFT,HEIGHT-120,CONTENT)
    top-=23;c.setFillColor(INK);c.setFont('Report-Bold',12)
    c.drawString(LEFT,top,f'Latest {EXERCISES[exercise].lower()} result - Session {attempt_number(rows,index)}')
    c.setFillColor(GREEN if r['status']=='Good' else ORANGE)
    c.setFont('Report-Bold',13);c.drawString(LEFT,top-22,result_label(r,minimum))
    top=paragraph(c,short_feedback(r,minimum),LEFT,top-32,CONTENT)
    top-=15
    bpm='Unavailable' if r['bpm_avg'] is None else f"{r['bpm_avg']:.1f}"
    cards=[('Recorded reps / goal',f"{r['rep_count']} / {r['target_reps']:g}"),('Active time',f"{r['active_seconds']:.1f}s"),('Mean BPM',bpm),('Beat samples',str(r['bpm_samples']))]
    for i,(label,value) in enumerate(cards):
        x=LEFT+i*(CONTENT/4);c.setFillColor(LIGHT);c.roundRect(x,top-57,CONTENT/4-6,57,5,stroke=0,fill=1)
        c.setFillColor(MUTED);c.setFont('Report',7);c.drawString(x+8,top-16,label)
        c.setFillColor(INK);c.setFont('Report-Bold',12);c.drawString(x+8,top-37,value)
    top-=83
    change=progression(rows,index,targets,minimum,rep_minimum)
    top=paragraph(c,progress_summary(rows,index,change)+'\n'+(progress_summary(rows,index,change,True) if change else ''),LEFT,top,CONTENT)
    scored=[evaluate(item,targets,minimum,rep_minimum) for _,item in entries]
    labels=[attempt_number(rows,i) for i,_ in entries]
    top-=30;half=(CONTENT-24)/2
    chart(c,LEFT,top,half,185,[v['rep_count'] for v in scored],labels,targets[exercise],'Recorded reps',unit=' reps')
    chart(c,LEFT+half+24,top,half,185,[v['active_seconds'] for v in scored],labels,None,'Active time (s)')


def _rep_summary(c,rows,entries,targets,minimum,rep_minimum):
    index,row=entries[-1];r=evaluate(row,targets,minimum,rep_minimum)
    top=paragraph(c,f"Good requires BOTH: at least {targets['chest_press']:g} good reps AND {rep_minimum:g}% good reps.",LEFT,HEIGHT-120,CONTENT)
    top-=23;c.setFillColor(INK);c.setFont('Report-Bold',12)
    c.drawString(LEFT,top,f'Latest chest-press result - Session {attempt_number(rows,index)}')
    c.setFillColor(GREEN if r['status']=='Good' else ORANGE if r['status']=='Not good yet' else MUTED)
    c.setFont('Report-Bold',13);c.drawString(LEFT,top-22,result_label(r,rep_minimum))
    top=paragraph(c,short_feedback(r,rep_minimum),LEFT,top-32,CONTENT)
    top-=15
    pct='Unavailable' if r['good_rep_percent'] is None else f"{r['good_rep_percent']:.1f}%"
    cards=[('Good reps / goal',f"{r['good_reps']} / {r['target_reps']:g}"),('Good reps (%)',pct),('Short / attempts',f"{r['short_attempts']} / {r['attempts']}"),('Active time',f"{r['active_seconds']:.1f}s")]
    for i,(label,value) in enumerate(cards):
        x=LEFT+i*(CONTENT/4);c.setFillColor(LIGHT);c.roundRect(x,top-57,CONTENT/4-6,57,5,stroke=0,fill=1)
        c.setFillColor(MUTED);c.setFont('Report',8);c.drawString(x+8,top-16,label)
        c.setFillColor(INK);c.setFont('Report-Bold',12);c.drawString(x+8,top-37,value)
    top-=72
    top=paragraph(c,f"Calibration: {row['start_cm']:.1f}cm start to {row['pressed_cm']:.1f}cm pressed. Travel {r['calibrated_travel_cm']:.1f}cm.\nRequired press {r['required_travel_cm']:.1f}cm; return within {r['return_tolerance_cm']:.1f}cm of start. Last completed travel {row['last_travel_cm']:.1f}cm.",LEFT,top,CONTENT)
    change=progression(rows,index,targets,minimum,rep_minimum)
    top=paragraph(c,progress_summary(rows,index,change)+'\n'+(progress_summary(rows,index,change,True) if change else ''),LEFT,top-12,CONTENT)
    scored=[evaluate(item,targets,minimum,rep_minimum) for _,item in entries]
    labels=[attempt_number(rows,i) for i,_ in entries]
    top-=27;half=(CONTENT-24)/2
    chart(c,LEFT,top,half,165,[v['good_reps'] for v in scored],labels,targets['chest_press'],'Good reps',unit=' reps')
    chart(c,LEFT+half+24,top,half,165,[v['good_rep_percent'] for v in scored],labels,rep_minimum,'Good reps (%)',True)

