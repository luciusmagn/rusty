<h1 align="center">RUSTY</h1>

<a href="#NAME">NAME</a><br>
<a href="#SYNOPSIS">SYNOPSIS</a><br>
<a href="#DESCRIPTION">DESCRIPTION</a><br>
<a href="#OPTIONS">OPTIONS</a><br>

<hr>


<h2>NAME
<a name="NAME"></a>
</h2>


<p style="margin-left:11%; margin-top: 1em">rusty &minus;
small and fast build system for C, C++ and possibly more</p>

<h2>SYNOPSIS
<a name="SYNOPSIS"></a>
</h2>


<p style="margin-left:11%; margin-top: 1em"><b>rusty</b>
[<b>&minus;h</b>] [<b>&minus;&minus;help</b>]
[<b>&minus;&minus;ast</b>] [<b>&minus;&minus;info</b>]
[<b>&minus;&minus;about</b>] [<b>&minus;viratwn</b>]
[<b>&minus;o</b> <i>DIR</i>] [<b>&minus;&minus;output</b>
<i>DIR</i>] [<b>&minus;d</b> <i>DIR</i>]
[<b>&minus;&minus;dir</b> <i>DIR</i>] [<b>&minus;c</b>
<i>COMPILER</i>] [<b>&minus;&minus;compiler</b>
<i>COMPILER</i>] [<b>&minus;o</b> <i>DIR</i>]
[<i>TARGETS</i>] [<b>clean</b>] [<b>install</b>]
[<b>uninstall</b>]</p>

<h2>DESCRIPTION
<a name="DESCRIPTION"></a>
</h2>


<p style="margin-left:11%; margin-top: 1em"><b>rusty</b> is
a simple build system, which borrowed its core syntax from
C2&rsquo;s built-in build system. Rusty uses Daniel
Holden&rsquo;s (orangeduck&rsquo;s) mpc. At the moment,
rusty can handle four types of targets: executables, shared
and static libraries and object-code only targets. Rusty
requires a rustyfile (rusty.txt) somewhere in the cwd path.
Rusty searches for it recursively, so you don&rsquo;t have
to worry about executing it from your projects&rsquo;
subdirectories.</p>

<h2>OPTIONS
<a name="OPTIONS"></a>
</h2>



<p style="margin-left:11%; margin-top: 1em"><b>&minus;h</b>,
<b>&minus;&minus;help</b></p>

<p style="margin-left:22%;">Print a basic help text.</p>

<p style="margin-left:11%;"><b>&minus;&minus;about</b></p>

<p style="margin-left:22%;">Print a basic about text with
even more basic usage example.</p>

<p style="margin-left:11%;"><b>&minus;c</b>,
<b>&minus;&minus;compiler</b> <i>name</i></p>

<p style="margin-left:22%;">Change the compiler used to
process files.</p>

<p style="margin-left:11%;"><b>&minus;i</b>,
<b>&minus;&minus;info</b></p>

<p style="margin-left:22%;">Print info about each target
found in the rustyfile.</p>

<p style="margin-left:11%;"><b>&minus;d</b>,
<b>&minus;&minus;dir</b> <i>path</i></p>

<p style="margin-left:22%;">Change directory before looking
for rustyfile.</p>

<p style="margin-left:11%;"><b>&minus;r</b>,
<b>&minus;&minus;fullrebuild</b></p>

<p style="margin-left:22%;">Recompile all files regardless
of whether or not has it been modified. Useful if
compilation fails, because atm, Rusty can&rsquo;t detect if
the compilation has failed or not.</p>

<p style="margin-left:11%;"><b>&minus;o</b>,
<b>&minus;&minus;output</b> <i>path</i></p>

<p style="margin-left:22%;">Change the output directory for
targets</p>

<p style="margin-left:11%;"><b>&minus;t</b>,
<b>&minus;&minus;time</b></p>

<p style="margin-left:22%;">Measure and print the CPU clock
time rusty&rsquo;s execution took. Note that the CPU time is
much shorter than the total time execution took.</p>

<p style="margin-left:11%;"><b>&minus;n</b>,
<b>&minus;&minus;check</b></p>

<p style="margin-left:22%;">Just check the rustyfile and
don&rsquo;t compile or build anything. Other
command&minus;line arguments still processed.</p>

<p style="margin-left:11%;"><b>&minus;w</b>,
<b>&minus;&minus;wanted&minus;only</b></p>

<p style="margin-left:22%;">Show info &amp; ast only for
desired targets, not the whole rustyfile. usage of &minus;i
and &minus;a is still required for the information to
show.</p>
<hr>
