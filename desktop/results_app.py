"""ECE198 Results: a Windows desktop app for Solution B's measured holds."""
import csv
import os
from pathlib import Path
import queue
import sys
import tempfile
import threading
import time
import webbrowser
import tkinter as tk
from tkinter import ttk, filedialog, messagebox

# Source checkout and PyInstaller both use these imports.
ROOT = Path(__file__).resolve().parents[1]
if str(ROOT / "tools") not in sys.path: sys.path.insert(0, str(ROOT / "tools"))
from export_sessions import read_history, write_csv
from evaluation import (EXERCISES, ResultStore, criteria_text, evaluate, load_csv,
                        normalize, progression, validate_criteria)
from presentation import result_label, short_feedback
from clean_report import build_report


def data_directory():
    base = Path(os.environ.get("LOCALAPPDATA", Path.home() / ".local" / "share"))
    return base / "ECE198_Results"


class ResultsApp:
    def __init__(self, root, store_path=None):
        self.root = root
        self.store = ResultStore(store_path or data_directory() / "results.sqlite3")
        self.rows = []
        self.confirmed = False
        self.source_label = "No results loaded"
        self.jobs = queue.Queue()
        self.busy = False
        root.title("ECE198 — Hold Results")
        width=min(1080,root.winfo_screenwidth()-60)
        height=min(900,root.winfo_screenheight()-65)
        root.geometry(f"{width}x{height}+20+20")
        root.minsize(860, 560)
        root.configure(bg="#f3f6fb")
        style=ttk.Style(root)
        style.theme_use("clam")
        style.configure(".", font=("Segoe UI", 10))
        style.configure("TFrame", background="#f3f6fb")
        style.configure("TLabel", background="#f3f6fb", foreground="#17283d")
        style.configure("Title.TLabel", font=("Segoe UI", 21, "bold"))
        style.configure("Heading.TLabel", font=("Segoe UI", 12, "bold"))
        style.configure("TButton", padding=(10, 7))
        style.configure("Treeview", rowheight=26, font=("Segoe UI", 10))
        style.configure("Treeview.Heading", font=("Segoe UI", 10, "bold"))
        style.configure("TLabelframe",background="#f3f6fb")
        style.configure("TLabelframe.Label",background="#f3f6fb",foreground="#17283d")
        viewport=tk.Canvas(root,bg="#f3f6fb",highlightthickness=0)
        main_scroll=ttk.Scrollbar(root,orient="vertical",command=viewport.yview)
        main_scroll.pack(side="right",fill="y");viewport.pack(side="left",fill="both",expand=True)
        viewport.configure(yscrollcommand=main_scroll.set)
        outer=ttk.Frame(viewport,padding=14)
        window=viewport.create_window((0,0),window=outer,anchor="nw")
        outer.bind("<Configure>",lambda event:viewport.configure(scrollregion=viewport.bbox("all")))
        viewport.bind("<Configure>",lambda event:viewport.itemconfigure(window,width=event.width))
        root.bind("<MouseWheel>",lambda event:viewport.yview_scroll(int(-event.delta/120),"units"))
        ttk.Label(outer, text="Hold results", style="Title.TLabel").pack(anchor="w")
        ttk.Label(outer, text="Measured duration, consistency and progress — wall sit / plank").pack(anchor="w", pady=(1,12))
        top=ttk.Frame(outer);top.pack(fill="x")
        ttk.Label(top, text="Participant label:").pack(side="left")
        self.person=tk.StringVar(value="Participant 1")
        self.people=ttk.Combobox(top, textvariable=self.person, values=self.store.participants(), width=23)
        self.people.pack(side="left",padx=8)
        ttk.Button(top,text="Open profile",command=self.open_profile).pack(side="left")
        ttk.Button(top,text="Manage participants",command=self.manage_participants).pack(side="left",padx=8)
        ttk.Button(top,text="Open results CSV",command=self.open_csv).pack(side="right")
        ttk.Button(top,text="Read Arduino",command=self.read_arduino).pack(side="right",padx=8)
        criteria=ttk.LabelFrame(outer,text="1. Set and confirm criteria",padding=10);criteria.pack(fill="x",pady=12)
        self.wall=tk.StringVar();self.plank=tk.StringVar();self.minimum=tk.StringVar(value="90")
        fields=[("Wall-sit target (s)",self.wall),("Plank target (s)",self.plank),("Required consistency (%)",self.minimum)]
        for i,(label,var) in enumerate(fields):
            ttk.Label(criteria,text=label).grid(row=0,column=i*2,sticky="w",padx=(0,6))
            ttk.Entry(criteria,textvariable=var,width=8).grid(row=0,column=i*2+1,padx=(0,16))
            var.trace_add("write", self.criteria_changed)
        ttk.Button(criteria,text="Review & confirm",command=self.review_criteria).grid(row=1,column=0,columnspan=2,sticky="w",pady=(8,0))
        self.criteria_note=tk.StringVar(value="Enter targets you have chosen. The 90% value is an editable prototype setting.")
        ttk.Label(criteria,textvariable=self.criteria_note).grid(row=1,column=2,columnspan=4,sticky="w",pady=(8,0))
        self.person.trace_add("write",self.participant_changed)
        list_header=ttk.Frame(outer);list_header.pack(fill="x")
        ttk.Label(list_header,text="2. Select an attempt",style="Heading.TLabel").pack(side="left")
        self.visible_rows=tk.StringVar(value="10" if root.winfo_screenheight()>=1000 else "7")
        row_picker=ttk.Combobox(list_header,textvariable=self.visible_rows,values=("7","10"),state="readonly",width=3)
        row_picker.pack(side="right");ttk.Label(list_header,text="Visible rows: ").pack(side="right")
        row_picker.bind("<<ComboboxSelected>>",lambda event:self.table.configure(height=int(self.visible_rows.get())))
        table=ttk.Frame(outer);table.pack(fill="both",expand=True,pady=(7,10))
        columns=("attempt","exercise","hold","consistency","result")
        self.table=ttk.Treeview(table,columns=columns,show="headings",height=int(self.visible_rows.get()),selectmode="browse")
        for key,title,width in zip(columns,("Attempt","Exercise","Held / target","Consistency","Result / reason"),(65,105,135,120,355)):
            self.table.heading(key,text=title);self.table.column(key,width=width,anchor="w")
        self.table.pack(side="left",fill="both",expand=True)
        scroll=ttk.Scrollbar(table,orient="vertical",command=self.table.yview);scroll.pack(side="right",fill="y")
        self.table.configure(yscrollcommand=scroll.set)
        self.table.bind("<<TreeviewSelect>>",lambda event:self.show_selected())
        detail=ttk.Frame(outer,padding=(0,6));detail.pack(fill="x")
        self.attempt_title=tk.StringVar(value="No attempt selected")
        ttk.Label(detail,textvariable=self.attempt_title,style="Heading.TLabel").pack(anchor="w",pady=(0,4))
        self.verdict=tk.Label(detail,text="Confirm criteria to evaluate",font=("Segoe UI",13,"bold"),bg="#f3f6fb",fg="#3d5870",anchor="w")
        self.verdict.pack(fill="x")
        self.stats=tk.StringVar(value="Select a session after importing results.")
        ttk.Label(detail,textvariable=self.stats,font=("Segoe UI",11),justify="left",wraplength=780).pack(anchor="w",pady=2)
        self.feedback=tk.StringVar()
        ttk.Label(detail,textvariable=self.feedback,wraplength=780,justify="left").pack(anchor="w")
        self.progress_text=tk.StringVar(value="Progress will compare the same exercise and tolerance under your confirmed criteria.")
        ttk.Label(detail,textvariable=self.progress_text,wraplength=780,justify="left",).pack(anchor="w",pady=(5,0))
        foot=ttk.Frame(outer);foot.pack(before=detail,fill="x",pady=(0,10))
        ttk.Button(foot,text="View progression",command=self.show_progress).pack(side="left")
        ttk.Button(foot,text="Clean report (PDF)",command=self.export_clean_report).pack(side="left",padx=8)
        ttk.Button(foot,text="Export CSV",command=self.export_report).pack(side="left")
        self.status=tk.StringVar(value="Ready. Local history is kept on this computer; use participant labels to keep people separate.")
        ttk.Label(outer,textvariable=self.status,wraplength=1000).pack(anchor="w",pady=(9,0))
        ttk.Label(outer,text="This evaluates measured proximity and duration, not full exercise form. Import time is not the session date.",foreground="#526578",wraplength=1000).pack(anchor="w",pady=(4,0))
        root.protocol("WM_DELETE_WINDOW",self.close)
        self.open_profile()
        root.after(100,self.check_jobs)

    def criteria_changed(self,*args):
        self.confirmed=False
        if hasattr(self,"criteria_note"):
            self.criteria_note.set("Criteria changed — review and confirm before evaluating.")
        if hasattr(self,"table"): self.refresh_table()

    def participant_changed(self,*args):
        self.confirmed=False
        if hasattr(self,"table"):
            self.rows=[];self.refresh_table()
            self.status.set("Click Open profile to load this participant, or import their results.")

    def criteria(self):
        targets={}
        for key,var in (("wall_sit",self.wall),("plank",self.plank)):
            try:targets[key]=float(var.get())
            except ValueError:raise ValueError(f"Enter a target time in seconds for {EXERCISES[key].lower()}.") from None
        try:minimum=float(self.minimum.get())
        except ValueError:raise ValueError("Enter a required consistency percentage.") from None
        validate_criteria(targets,minimum)
        return targets,minimum

    def open_profile(self):
        name=self.person.get().strip()
        if not name: return
        saved=self.store.criteria(name)
        if saved:
            self.wall.set(str(saved["targets"]["wall_sit"]))
            self.plank.set(str(saved["targets"]["plank"]))
            self.minimum.set(str(saved["minimum"]))
        else:
            self.wall.set("");self.plank.set("");self.minimum.set("90")
        self.confirmed=False
        self.rows=self.store.load(name)
        self.refresh_table()
        self.people.configure(values=self.store.participants())
        self.status.set(f"{name}: {len(self.rows)} saved attempt(s). Confirm criteria to see evaluations.")

    def review_criteria(self):
        if not self.person.get().strip():
            messagebox.showerror("Participant","Enter a participant label first.",parent=self.root);return
        try: targets,minimum=self.criteria()
        except (ValueError,TypeError) as error:
            messagebox.showerror("Check criteria",str(error),parent=self.root);return
        dialog=tk.Toplevel(self.root);dialog.title("Confirm evaluation criteria");dialog.transient(self.root)
        dialog.geometry("760x520");dialog.grab_set()
        ttk.Label(dialog,text="Please confirm before evaluating",style="Heading.TLabel").pack(anchor="w",padx=18,pady=14)
        ttk.Label(dialog,text=criteria_text(targets,minimum),wraplength=680,justify="left").pack(anchor="w",padx=18)
        buttons=ttk.Frame(dialog,padding=18);buttons.pack(side="bottom",fill="x")
        ttk.Button(buttons,text="Back",command=dialog.destroy).pack(side="left")
        def confirm():
            self.accept_criteria(targets,minimum)
            dialog.destroy()
        ttk.Button(buttons,text="Confirm and evaluate",command=confirm).pack(side="right")

    def accept_criteria(self,targets,minimum):
        self.store.save_criteria(self.person.get().strip(),targets,minimum)
        self.confirmed=True
        self.criteria_note.set(f"Confirmed: wall sit {targets['wall_sit']:g}s; plank {targets['plank']:g}s; consistency ≥{minimum:g}%.")
        self.refresh_table()

    def open_csv(self):
        path=filedialog.askopenfilename(title="Open saved sessions",filetypes=[("CSV results","*.csv")])
        if not path:return
        try:self.preview_import(load_csv(path),str(path))
        except (ValueError,OSError,KeyError,csv.Error) as error:messagebox.showerror("Could not read results",str(error),parent=self.root)

    def preview_import(self,rows,source):
        if not rows:messagebox.showinfo("No saved sessions","This source has no saved sessions.",parent=self.root);return
        if not self.person.get().strip():messagebox.showerror("Participant","Enter a participant label first.",parent=self.root);return
        dialog=tk.Toplevel(self.root);dialog.title("Choose sessions to keep");dialog.geometry("850x440");dialog.transient(self.root);dialog.grab_set()
        ttk.Label(dialog,text=f"Keep only {self.person.get().strip()}'s sessions selected. Ctrl-click to select/deselect.",wraplength=800).pack(anchor="w",padx=14,pady=14)
        preview=ttk.Frame(dialog);preview.pack(fill="both",expand=True,padx=14)
        tree=ttk.Treeview(preview,columns=("id","exercise","active","hold","tail","band"),show="headings",selectmode="extended",height=7)
        for key,label in zip(tree["columns"],("Session","Exercise","Active (s)","In band (s)","Ending excluded (s)","Tolerance (cm)")):
            tree.heading(key,text=label);tree.column(key,width=125)
        tree.pack(side="left",fill="both",expand=True)
        scrollbar=ttk.Scrollbar(preview,orient="vertical",command=tree.yview)
        scrollbar.pack(side="right",fill="y");tree.configure(yscrollcommand=scrollbar.set)
        for i,row in enumerate(rows):
            tail="Unknown" if row["trailing_out_ms"] is None else f"{row['trailing_out_ms']/1000:.2f}"
            tree.insert("","end",iid=str(i),values=(row["session_id"],EXERCISES[row["exercise"]],f"{row['active_ms']/1000:.2f}",f"{row['in_band_ms']/1000:.2f}",tail,f"±{row['band_cm']:g}"))
        tree.selection_set(tree.get_children())
        buttons=ttk.Frame(dialog,padding=14);buttons.pack(fill="x")
        ttk.Button(buttons,text="Cancel",command=dialog.destroy).pack(side="left")
        def keep():
            selected=[rows[int(i)] for i in tree.selection()]
            if not selected: return
            added=self.store.add(self.person.get().strip(),selected)
            self.rows=self.store.load(self.person.get().strip());self.refresh_table()
            self.people.configure(values=self.store.participants())
            self.status.set(f"Added {added} new attempt(s); duplicates skipped. Saved on this computer. Source: {source}")
            dialog.destroy()
        ttk.Button(buttons,text="Keep selected sessions",command=keep).pack(side="right")

    def refresh_table(self):
        selected=self.selected_index()
        self.table.delete(*self.table.get_children())
        criteria=self.criteria() if self.confirmed else None
        for i,row in enumerate(self.rows):
            hold=f"{row['in_band_ms']/1000:.1f}s"
            consistency="Not evaluated";status="Confirm criteria"
            if criteria:
                result=evaluate(row,*criteria);value=result["consistency_percent"]
                hold+=f" / {result['target_seconds']:g}s"
                consistency="Unavailable" if value is None else f"{value:.1f}% / {criteria[1]:g}%"
                status=result_label(result,criteria[1])
            self.table.insert("","end",iid=str(i),values=(i+1,EXERCISES[row["exercise"]],hold,consistency,status))
        if self.rows:
            index=selected if selected is not None and selected<len(self.rows) else len(self.rows)-1
            self.table.selection_set(str(index));self.table.see(str(index))
        self.show_selected()

    def selected_index(self):
        values=self.table.selection();return int(values[0]) if values else None

    def show_selected(self):
        index=self.selected_index()
        if index is None:
            self.attempt_title.set("No attempt selected")
            self.verdict.configure(text="Import results to begin",fg="#3d5870")
            self.stats.set("Open a CSV or read the Arduino to add results.");self.feedback.set("")
            self.progress_text.set("");return
        row=self.rows[index]
        self.attempt_title.set(f"{EXERCISES[row['exercise']]} - Attempt {index+1} - target zone ±{row['band_cm']:g} cm")
        if not self.confirmed:
            self.verdict.configure(text="Confirm criteria first",fg="#3d5870")
            self.stats.set(f"Time in target zone: {row['in_band_ms']/1000:.1f}s | Active: {row['active_ms']/1000:.1f}s")
            self.feedback.set("Enter both targets, then click Review & confirm.")
            self.progress_text.set("");return
        targets,minimum=self.criteria();r=evaluate(row,targets,minimum)
        color={"Good":"#16734a","Not good yet":"#ae4b1a","Cannot evaluate":"#586b7e"}[r["status"]]
        self.verdict.configure(text=result_label(r,minimum),fg=color)
        pct="Unavailable" if r["consistency_percent"] is None else f"{r['consistency_percent']:.1f}%"
        self.stats.set(f"Held {r['in_band_seconds']:.1f}s / goal {r['target_seconds']:g}s | Consistency {pct} / goal {minimum:g}%")
        self.feedback.set(short_feedback(r,minimum))
        change=progression(self.rows,index,targets,minimum)
        if change:
            self.progress_text.set(f"Since previous {EXERCISES[row['exercise']].lower()} attempt {change['previous_index']+1}: held {change['hold_change']:+.1f}s; consistency {change['consistency_change']:+.1f} percentage points.")
        else:self.progress_text.set("No earlier comparable scored attempt yet.")

    def delete_profile(self, name):
        """Delete the complete local profile. The caller owns user confirmation."""
        count=self.store.delete_participant(name)
        self.people.configure(values=self.store.participants())
        if self.person.get().strip()==name:
            self.person.set("");self.wall.set("");self.plank.set("");self.minimum.set("90")
            self.rows=[];self.confirmed=False;self.refresh_table()
        self.status.set(f"Deleted {name} and {count} local attempt(s). Exported files and Arduino history were kept.")
        return count

    def manage_participants(self):
        dialog=tk.Toplevel(self.root);dialog.title("Manage participants");dialog.geometry("650x500")
        dialog.transient(self.root);dialog.grab_set()
        body=ttk.Frame(dialog,padding=14);body.pack(fill="both",expand=True)
        ttk.Label(body,text="Search participant labels",style="Heading.TLabel").pack(anchor="w")
        query=tk.StringVar();search=ttk.Entry(body,textvariable=query);search.pack(fill="x",pady=8)
        note=tk.StringVar();ttk.Label(body,textvariable=note).pack(anchor="w",pady=(0,8))
        area=ttk.Frame(body);area.pack(fill="both",expand=True)
        tree=ttk.Treeview(area,columns=("participant","count"),show="headings",height=9,selectmode="browse")
        tree.heading("participant",text="Participant");tree.heading("count",text="Saved attempts")
        tree.column("participant",width=410);tree.column("count",width=120)
        tree.pack(side="left",fill="both",expand=True)
        scroll=ttk.Scrollbar(area,orient="vertical",command=tree.yview);scroll.pack(side="right",fill="y");tree.configure(yscrollcommand=scroll.set)
        visible=[]
        def refresh(*args):
            tree.delete(*tree.get_children());visible[:]=self.store.participant_summaries(query.get(),limit=100)
            for i,row in enumerate(visible):tree.insert("","end",iid=str(i),values=(row['participant'],row['attempts']))
            total=self.store.participant_count()
            note.set(f"{total} saved participant(s). Showing up to 100 matches; type to narrow the list.")
        query.trace_add("write",refresh)
        def selected():
            ids=tree.selection();return visible[int(ids[0])] if ids else None
        def open_selected():
            row=selected()
            if row:self.person.set(row['participant']);self.open_profile();dialog.destroy()
        def delete_selected():
            row=selected()
            if not row:return
            if messagebox.askyesno("Delete participant?",f"Delete {row['participant']}?\n\nThis removes the profile, saved criteria and all {row['attempts']} local attempt(s). This cannot be undone.\nExported files and Arduino records stay untouched.",parent=dialog,default="no"):
                self.delete_profile(row['participant']);refresh()
        buttons=ttk.Frame(body);buttons.pack(fill="x",pady=(12,0))
        ttk.Button(buttons,text="Delete participant",command=delete_selected).pack(side="left")
        ttk.Button(buttons,text="Open selected",command=open_selected).pack(side="right")
        ttk.Button(buttons,text="Close",command=dialog.destroy).pack(side="right",padx=8)
        tree.bind("<Double-1>",lambda event:open_selected())
        refresh();search.focus_set()
        return dialog

    def export_clean_report(self):
        if not self.confirmed:
            messagebox.showinfo("Confirm criteria","Review and confirm criteria before creating a report.",parent=self.root);return
        if not self.rows:
            messagebox.showinfo("No attempts","Import saved attempts first.",parent=self.root);return
        path=filedialog.asksaveasfilename(title="Save readable progress report",defaultextension=".pdf",initialfile="exercise_progress.pdf",filetypes=[("PDF report","*.pdf")])
        if not path:return
        try:
            build_report(path,self.person.get().strip(),self.rows,*self.criteria())
            self.status.set(f"PDF report saved: {path}")
            webbrowser.open(Path(path).resolve().as_uri())
        except FileExistsError:messagebox.showerror("File exists","Choose a new filename; existing reports are preserved.",parent=self.root)
        except (OSError,ValueError) as error:messagebox.showerror("Report failed",str(error),parent=self.root)

    def show_progress(self):
        if not self.confirmed:messagebox.showinfo("Confirm criteria","Review and confirm criteria first.",parent=self.root);return
        index=self.selected_index()
        if index is None:return
        current=self.rows[index];targets,minimum=self.criteria()
        rows=[r for r in self.rows if r['exercise']==current['exercise'] and abs(r['band_cm']-current['band_cm'])<0.001]
        scored=[(row,evaluate(row,targets,minimum)) for row in rows]
        scored=[(row,r) for row,r in scored if r['consistency_percent'] is not None]
        dialog=tk.Toplevel(self.root);dialog.title("Progression — "+EXERCISES[current['exercise']]);dialog.geometry("900x520")
        ttk.Label(dialog,text=f"{self.person.get()} · {EXERCISES[current['exercise']]} · ±{current['band_cm']:g} cm",style="Heading.TLabel").pack(anchor="w",padx=16,pady=10)
        ttk.Label(dialog,text="All attempts use the current confirmed criteria. Trial order, not calendar dates.").pack(anchor="w",padx=16)
        canvas=tk.Canvas(dialog,bg="white",highlightthickness=0);canvas.pack(fill="both",expand=True,padx=16,pady=12)
        def draw(event=None):
            canvas.delete("all");w=max(canvas.winfo_width(),800);h=max(canvas.winfo_height(),350)
            if not scored:canvas.create_text(w/2,h/2,text="No sessions with final-interval data yet.",font=("Segoe UI",13));return
            left,right,top,bottom=65,w-35,30,h-70
            canvas.create_line(left,top,left,bottom,right,bottom,fill="#9aa8b5")
            canvas.create_text(left,15,text="Consistency (%)",anchor="w",fill="#24445e")
            for value in (0,25,50,75,100):
                y=bottom-(bottom-top)*value/100;canvas.create_line(left,y,right,y,fill="#e5ebf0");canvas.create_text(left-10,y,text=str(value),anchor="e")
            threshold=bottom-(bottom-top)*minimum/100
            canvas.create_line(left,threshold,right,threshold,fill="#cf9f39",dash=(5,4))
            pts=[]
            for i,(row,r) in enumerate(scored):
                x=left+(right-left)*(i/(len(scored)-1) if len(scored)>1 else 0.5)
                y=bottom-(bottom-top)*r['consistency_percent']/100;pts.extend((x,y))
                canvas.create_oval(x-4,y-4,x+4,y+4,fill="#176eaa",outline="")
                if len(scored)<=12:canvas.create_text(x,bottom+17,text=f"{i+1}")
            if len(pts)>2:canvas.create_line(*pts,fill="#176eaa",width=2)
            canvas.create_text((left+right)/2,h-20,text="Attempt order · dots show measured consistency · dashed line is your requirement",fill="#3e576b")
        canvas.bind("<Configure>",draw);dialog.after(100,draw)

    def export_report(self):
        if not self.confirmed:messagebox.showinfo("Confirm criteria","Confirm criteria before exporting an assessment.",parent=self.root);return
        if not self.rows:return
        path=filedialog.asksaveasfilename(title="Save assessment report",defaultextension=".csv",initialfile="hold_results.csv",filetypes=[("CSV","*.csv")])
        if not path:return
        if Path(path).exists():messagebox.showerror("File exists","Choose a new filename; existing reports are preserved.",parent=self.root);return
        targets,minimum=self.criteria()
        columns=["participant","session_id","history_id","exercise","status","active_seconds","in_band_seconds","trailing_seconds","adjusted_active_seconds","uncredited_seconds","consistency_percent","target_seconds","completion_percent","required_consistency_percent","feedback"]
        try:
            with Path(path).open("x",encoding="utf-8-sig",newline="") as out:
                writer=csv.DictWriter(out,fieldnames=columns);writer.writeheader()
                for row in self.rows:
                    r=evaluate(row,targets,minimum)
                    item={k:r.get(k,"") for k in columns};item.update(participant=self.person.get(),session_id=row['session_id'],history_id=row['history_id'],exercise=row['exercise'],required_consistency_percent=minimum,feedback=" ".join(r['reasons']))
                    writer.writerow(item)
            self.status.set(f"Report saved: {path}")
        except OSError as error:messagebox.showerror("Save failed",str(error),parent=self.root)

    def read_arduino(self):
        if self.busy:return
        try:
            import serial.tools.list_ports
            ports=list(serial.tools.list_ports.comports())
        except ImportError:messagebox.showerror("Serial support","The source version needs pyserial. The Windows executable includes it.",parent=self.root);return
        dialog=tk.Toplevel(self.root);dialog.title("Read saved Arduino sessions");dialog.geometry("570x250");dialog.transient(self.root);dialog.grab_set()
        ttk.Label(dialog,text="Finish and save the session first. Close Arduino Serial Monitor.\nConnecting may reset the Uno; saved summaries remain.",wraplength=530).pack(anchor="w",padx=16,pady=16)
        port=tk.StringVar(value=ports[0].device if ports else "COM10")
        box=ttk.Combobox(dialog,textvariable=port,values=[p.device for p in ports]);box.pack(anchor="w",padx=16)
        ttk.Label(dialog,text="Select the Solution B Arduino's port.").pack(anchor="w",padx=16,pady=8)
        def read():
            device=port.get().strip()
            if not device:return
            dialog.destroy();self.busy=True;self.status.set("Reading saved sessions…")
            def worker():
                try:
                    import serial
                    with serial.Serial(device,115200,timeout=0.25) as connection:
                        time.sleep(2);connection.reset_input_buffer();rows=read_history(connection)
                    self.jobs.put(("rows",[normalize(row) for row in rows],device))
                except Exception as error:self.jobs.put(("error",str(error),device))
            threading.Thread(target=worker,daemon=True).start()
        ttk.Button(dialog,text="Read saved results",command=read).pack(anchor="e",padx=16,pady=12)

    def check_jobs(self):
        try:
            kind,value,source=self.jobs.get_nowait();self.busy=False
            if kind=="error":
                self.status.set("Could not read the board.")
                messagebox.showerror("Read failed",str(value)+"\nClose Serial Monitor, check the port and retry.",parent=self.root)
            else:self.preview_import(value,source)
        except queue.Empty:pass
        self.root.after(100,self.check_jobs)

    def close(self):
        self.store.close();self.root.destroy()


def smoke_test():
    with tempfile.TemporaryDirectory() as directory:
        root=tk.Tk();root.withdraw();app=ResultsApp(root,Path(directory)/"test.sqlite3")
        rows=[]
        for i,(exercise,inside,active) in enumerate([
            ("wall_sit",18000,35000),("plank",18000,23000),
            ("wall_sit",26000,40000),("plank",27000,35000),
            ("wall_sit",28000,35000),("plank",26000,40000),
            ("wall_sit",29000,35000),("plank",28000,35000),
            ("wall_sit",29500,35000),("plank",24000,35000)],1):
            rows.append(normalize(dict(session_id=i,exercise=exercise,active_ms=active,in_band_ms=inside,reference_cm=50,band_cm=10,trailing_out_ms=5000,has_hold=1,history_id=1)))
        app.store.add("Participant 1",rows);app.rows=app.store.load("Participant 1")
        app.wall.set("25");app.plank.set("25");app.minimum.set("90")
        assert not app.confirmed
        app.accept_criteria(*app.criteria());root.update()
        assert len(app.table.get_children())==10
        assert int(app.table.cget("height"))>=7
        assert "time + consistency" in app.verdict.cget("text")
        assert "Plank - Attempt 10" in app.attempt_title.get()
        assert "previous plank attempt" in app.progress_text.get()
        app.table.selection_set("8");app.show_selected();root.update()
        assert app.verdict.cget("text")=="Good - both goals met"
        assert "Wall sit - Attempt 9" in app.attempt_title.get()
        report=Path("clean_report_preview.pdf") if os.environ.get("ECE198_CAPTURE_PREVIEW") else Path(directory)/"report.pdf"
        if report.exists():report.unlink()  # Synthetic smoke-test output only.
        build_report(report,"Participant 1",app.rows,*app.criteria())
        assert report.stat().st_size>3000
        manager=app.manage_participants();root.update();manager.destroy()
        app.store.add("Temporary profile",rows[:1]);app.store.save_criteria("Temporary profile",*app.criteria())
        assert app.delete_profile("Temporary profile")==1
        assert not app.store.criteria("Temporary profile")
        assert not app.store.participant_summaries("Temporary profile")
        if os.environ.get("ECE198_CAPTURE_PREVIEW"):
            root.deiconify();root.update();root.after(500);root.update()
            from PIL import ImageGrab
            x,y=root.winfo_rootx(),root.winfo_rooty()
            ImageGrab.grab(bbox=(x,y,x+root.winfo_width(),y+root.winfo_height())).save("app_preview.png")
        app.show_progress();root.update()
        app.delete_profile("Participant 1")
        assert app.person.get()=="" and not app.rows and not app.store.participants()
        assert not app.store.criteria("Participant 1")
        app.close()


if __name__=="__main__":
    if "--smoke-test" in sys.argv:smoke_test()
    else:
        root=tk.Tk()
        try:ResultsApp(root)
        except Exception as error:
            messagebox.showerror("Could not start Hold Results",str(error),parent=root)
            root.destroy();raise SystemExit(1)
        root.mainloop()
