\subsection{Electro-convective instability}\label{sec:alimani}
We next demonstrate electrokinetic transport near an ion-selective surface, which is a good benchmarking problem because 1) the problem can be found in a wide range of applications, 2) the length scale of the EDL near the surface requires an extremely small mesh size, which makes the problem a good test case for the proposed octree-based mesh, and 3) The chaotic nature of the flow triggers structures over a wide range of scales in all field quantities, necessitating numerical stabilization from VMS.
%the turbulence-like characteristics trigger numerical instabilities, which necessitates numerical stabilization from VMS. 
We compare our results against the benchmark results of~\citep{druzgalski2013direct}.  

The simulation was performed for the transport of monovalent binary electrolytes in a rectangular domain whose dimension is 8 $\times$ 1. The top boundary is a reservoir at which the potential and the concentration of both species are fixed (Dirichlet). The ion-selective surface is located at the bottom boundary, where the potential is grounded. The cation concentration was $c^{+}=2$, and no anion flux was allowed across the ion-selective surface. A periodic boundary condition is applied to the side walls. \figref{fig:electroconvective-schematic} shows the schematic of the domain, and \tabref{tab:BC-AliMani} lists the boundary conditions for the different variables. We consider two cases, one when a moderate potential difference of $\Delta \phi = 20$ is imposed across the domain and another when a large potential difference of $\Delta \phi = 120$ is imposed.  We anticipate significant electro-convective instabilities to occur in the second case. 

% Here, we demonstrate the ability of the framework to capture the electroconvective instability generated as a resultant of the interplay between hydrodynamic and electrostatic forces. The instability manifests itself as chaotic fingering in the charge density, beyond a critical applied voltage, from the ion selective membranes surface. The accurate capture of physics require a very fine resolution of the order of Debye length near the ion-selective surface. 

% In order to study the electroconvective instability, we consider the model problem of ~\citet{druzgalski2013direct}. We consider a long rectangular channel of length 8 $\times$ 1 , with top boundary representing the fixed cation concentration (stationary reservoir) and the bottom boundary being impermeable to the anions and freely permeable to the cations (ion - selective surface). Periodic boundary condition is applied to the tangential boundaries of the domain. A potential difference of $\Delta \phi$ is applied between the reservoir and the ion-selective surface. \figref{fig:electroconvective-schematic} shows the schematic diagram of the domain.
\begin{figure}
\centering
% \begin{center}
%		\begin{tikzpicture}
%			\newcommand\starty{0.}
%			\draw[blue] (-4,\starty) -- (4,\starty);
%			\draw[dashed] (-4,\starty + 0) -- (-4,\starty +2);
%			\draw (-4,\starty +2) -- (4,\starty + 2);
%			\draw[dashed] (4,\starty +0) -- (4,\starty + 2);
%			
%			\draw (0,\starty + 2) -- (0,\starty + 2 +0.3);
%			\draw (-0.2,\starty+2+0.3) -- (0.2,\starty + 2 +0.3);
%			
%			\draw (0,\starty) -- (0,\starty - 0.3);
%			\draw (-0.2,\starty-0.3) -- (0.2,\starty - 0.3);
%			\draw (-0.15,\starty-0.35) -- (0.15,\starty - 0.35);
%			\draw (-0.10,\starty-0.40) -- (0.10,\starty - 0.40);
%			\draw (-0.05,\starty-0.45) -- (0.05,\starty - 0.45);
%			
%			\node[] at (0,\starty + 2 + 0.45) {\tiny{$\Delta \phi$}};
%			\node[] at (-2.5,\starty + 2 +0.2) {\scriptsize{Stationary reservoir} };
%			\node[] at (-2.5,\starty -0.2) {\scriptsize{Ion-selective surface} };
%			\node[rotate = 90] at (-4.2,\starty +1) {\scriptsize{Periodic} };
%			\node[rotate = 90] at (4.2,\starty +1) {\scriptsize{Periodic} };
%		\end{tikzpicture}
\includegraphics{methods_paper-figure4.pdf}
\caption{Schematic diagram (not on scale) for the electro-convective instability. }
\label{fig:electroconvective-schematic}
% \end{center}
\end{figure}

% Please add the following required packages to your document preamble:
% \usepackage{multirow}
\begin{table}[htb!]
\centering
\renewcommand{\arraystretch}{1.2}
\begin{tabular}{|ccccc|}
\hline
\multicolumn{2}{|c|}{} & \multicolumn{1}{c|}{\textbf{Variable}} &\multicolumn{1}{c|}{\textbf{Type}} & \textbf{Value} \\ \hline
\hline
\multicolumn{1}{|c|}{\multirow{5}{*}{NS}} & \multicolumn{1}{c|}{\multirow{2}{*}{Top Wall}} & \multicolumn{1}{c|}{u} & \multicolumn{1}{c|}{Dirichlet} & 0 \\ \cline{3-5} 
\multicolumn{1}{|c|}{} & \multicolumn{1}{c|}{} & \multicolumn{1}{c|}{v} & \multicolumn{1}{c|}{Dirichlet} & 0 \\ \cline{2-5} 
\multicolumn{1}{|c|}{} & \multicolumn{1}{c|}{\multirow{2}{*}{Bottom Wall}} & \multicolumn{1}{c|}{u} & \multicolumn{1}{c|}{Dirichlet} & 0 \\ \cline{3-5} 
\multicolumn{1}{|c|}{} & \multicolumn{1}{c|}{} & \multicolumn{1}{c|}{v} & \multicolumn{1}{c|}{Dirichlet} & 0 \\ \cline{2-5} 
\multicolumn{1}{|c|}{} & \multicolumn{1}{l|}{Side walls} & \multicolumn{3}{c|}{Periodic} \\ \hline
\hline
\multicolumn{1}{|c|}{\multirow{7}{*}{PNP}} & \multicolumn{1}{c|}{\multirow{3}{*}{Top Wall}} & \multicolumn{1}{c|}{$\phi$} & \multicolumn{1}{c|}{Dirichlet} & 20  or 120\\ \cline{3-5} 
\multicolumn{1}{|c|}{} & \multicolumn{1}{c|}{} & \multicolumn{1}{c|}{$c_{+}$} & \multicolumn{1}{c|}{Dirichlet} & 1 \\ \cline{3-5} 
\multicolumn{1}{|c|}{} & \multicolumn{1}{c|}{} & \multicolumn{1}{c|}{$c_{-}$} & \multicolumn{1}{c|}{Dirichlet} & 1 \\ \cline{2-5} 
\multicolumn{1}{|c|}{} & \multicolumn{1}{c|}{\multirow{3}{*}{Bottom Wall}} & \multicolumn{1}{c|}{$\phi$} & \multicolumn{1}{c|}{Dirichlet} & 0 \\ \cline{3-5} 
\multicolumn{1}{|c|}{} & \multicolumn{1}{c|}{} & \multicolumn{1}{c|}{$c_{+}$} & \multicolumn{1}{c|}{Dirichlet} & 2 \\ \cline{3-5} 
\multicolumn{1}{|c|}{} & \multicolumn{1}{c|}{} & \multicolumn{1}{c|}{$c_{-}$} & \multicolumn{1}{c|}{Zero flux} & 0 \\ \cline{2-5} 
\multicolumn{1}{|c|}{} & \multicolumn{1}{l|}{Side walls} & \multicolumn{3}{c|}{Periodic} \\ \hline
\end{tabular}
\caption{Boundary condition for the electro-convective instabilty}
\label{tab:BC-AliMani}
\end{table}
% Please add the following required packages to your document preamble:
% \usepackage{multirow}

\begin{table}[h]
%\added{
\centering
%begin{tabular}{|p{0.1\linewidth}|p{0.2\linewidth}|p{0.4\linewidth}|p{0.2\linewidth}|p{0.1\linewidth}|p{0.1\linewidth}|}
\begin{tabular}{|c|c|c|l|l|c|}
\hline
$\mathbf{\Delta \phi}$  & \textbf{Mesh size}                                         & \textbf{\# of MPI tasks}                                        & \textbf{\# of timestep} & \textbf{Final time}    & \textbf{Wall time} \\ \hline
$20$ & \multirow{2}{*}{\begin{tabular}[c]{@{}c@{}} $\sim$ 1M\\ (8.3 M equivalent)\end{tabular}} & \multirow{2}{*}{\begin{tabular}[c]{@{}c@{}}224 CPU processes\\ (4 nodes on TACC Frontera)\end{tabular}}  & 1000 & 10 & 5 hours    \\ \cline{1-1} \cline{4-6} 
$120$ &                                                                                  &                                                                                               & 12000    & 1.2 & 4 days     \\ \hline
\end{tabular}
\caption{Mesh details and time taken for the electro-convective instability.}
\label{table: mesh_AliMani}
%}
\end{table}

Similar to the previous example, finer refinement was used near the ion-selective surface to resolve steep gradients in the boundary layer. The bottom part of the domain, extending from y = 0 to y = 0.1, consists of elements refined to level 13 ($\Delta x = \Delta y =  8/2^{13}$), while the rest of the region consists of elements refined to level 10 ($\Delta x = \Delta y =  8/2^{10}$). The setup resulted in a domain with approximately 1M cells. In contrast, an equivalent uniform mesh at the finest resolution would have $\sim~10$M cells. The initial condition with fully developed EDL was generated by solving only the PNP equations, then randomly perturbing the resultant concentrations locally by 1\% to initiate the instability before solving the fully coupled NS-PNP equations.
The timestep was $10^{-2}$ for the case with potential difference, $\Delta \phi = 20$, and a time step of $10^{-4}$ for the case of potential difference $\Delta \phi = 120$. We ran the case of $\Delta \phi = 20$ until about 10 seconds when it reached steady-state (approximately 1000 timesteps). For the case of $\Delta \phi = 120$ we ran the simulation for 1.2 seconds, which encompasses the time to reach statistical stationarity and the time used to collect data (approximately 12000 timesteps). \tabref{table: mesh_AliMani} briefs the mesh details along with the number of MPI tasks and the wall time elapsed for both the cases.

We first compare the temporal evolution of the charge density ($c^{+} - c^{-}$) and velocity fields under the imposition of the potential drops. This is shown in \figref{fig:plot20V} for the $\Delta \phi = 20$ case and \figref{fig:plot120V} for the $\Delta \phi = 120$ case. Once a potential drop is applied, the ion concentration profile near the ion-selective surface forms an electric double layer (EDL) and diffusion layer (DL). Depending on the level of the potential drop, the extended space charge layer (ESC) may form between EDL and DL~\citep{druzgalski2013direct}. When $\Delta \phi=20$, the ESC is not formed, and the magnitude of convection is insufficient to trigger instabilities. A series of counter-rotating vortices are observed in \figref{fig:plot20V}.% in conjunction with the tangential perturbation of ESC.
~On the other hand, an ESC was observed in the initial stage of $\Delta \phi=120$. The interplay between ESC and convection triggered electrohydrodynamic instability, as reported in~\citet{druzgalski2013direct}. As the simulation proceeds, the development of electrohydrodynamic instability becomes even more evident. We note that the average velocity at $\Delta \phi = 120$ was greater than that of $\Delta \phi = 20$ case by two orders of magnitude.

We next extract quantitative properties for comparison. The current measured on the boundary is an often used property, as it is the easiest property to measure experimentally.~\figref{fig: AliManiCase} (a) shows the temporal variation of the magnitude of the net species flux, $|j_{T}^{\pm}|$, through the top boundary, calculated as 
\begin{equation}
j_{T}^{\pm}= - \frac{1}{L_{T}}\sum_{l=1}^{N_{T}}\left[\left(q, \, \hat{n}_T \left(\pd{c^{+, t}}{x_i} + z^{+}c^{+, t}\;\pd{\phi^{t}}{x_i}\right) \right)_{\Gamma_{l}}-\left(q, \, \hat{n}_T \left(\pd{c^{-, t}}{x_i} + z^{-}c^{-, t}\;\pd{\phi^{t}}{x_i}\right) \right)_{\Gamma_{l}}\right],
\end{equation}
where ${L_{T}}$, $N_{T}$, $\hat{n}_T$, and $\Gamma=\partial{\Omega_{\Gamma}}$ are the length of the top boundary, the number of the top boundary elements, the normal vector to the top boundary, and the top boundary, respectively. 
We see that, in the absence of electro-convective instabilities, the current (for the $\Delta \phi = 20$ case) was uniform over time, as expected in the absence of ESC and not enough convection to perturb established EDL. In contrast, significant current fluctuations were observed for the $\Delta \phi = 120$ case due to the instability.

We finally plot $x-$averaged field values for the potential (\figref{fig: AliManiCase} (b)), concentration (\figref{fig: AliManiCase} (c)) and charge density (\figref{fig: AliManiCase} (d)). Note the  logarithmic axis in these figures to ensure that the variation close to the ion-selective surface is emphasized. These figures also allow detailed comparison with previous DNS simulations of the same case by ~\citet{druzgalski2013direct}, which shows excellent agreement. 
%The higher concentration and potential gradient at $y=1$ support the higher current at $\Delta \phi = 120$ in ~\figref{fig: AliManiCase} (a). The charge density distribution (\figref{fig: AliManiCase} (d)) shows the influence from strong convection, which was represented by extended non-zero value away from the bottom surface. Again, the distribution of the charge density shows good agreement with what's reported in the literature.
Note that our numerical scheme allows three orders of magnitude larger time steps as compared to the benchmarking problem by ~\citet{druzgalski2013direct}, with identical results.


% \input{tikz/AliMani/AliMani_repr}

% \begin{landscape}

\newcommand \figwidthCase{20}
\begin{figure}[htb!]
\begin{tabular}{|l|l|l|l|}
\hline
& 
\parbox[c]{\figwidthCase em}{
\includegraphics[width=\linewidth,trim=100 1200 100 100,clip]{CDt0pBar.png}}
%  & 
%     \parbox[c]{\figwidthCase em}{
%       \includegraphics[width=\linewidth,trim=100 1200 100 100,clip]{Figures/AliMani/120V/ES/ESBar.png}}
& 
\parbox[c]{\figwidthCase em}{
\includegraphics[width=\linewidth,trim=100 1300 110 50,clip]{velocityBar.png}}
\\ \hline
\rotatebox[origin=c]{90}{t = 0.01} 
& 
\parbox[c]{\figwidthCase em}{
\includegraphics[width=\linewidth,trim=100 550 100 500,clip]{CD0p01.png}}
&
% \parbox[c]{\figwidthCase em}{
%   \includegraphics[width=\linewidth,trim=100 550 100 500,clip]{Figures/AliMani/20V/ES/ESt0p01.png}}   
% &
\parbox[c]{\figwidthCase em}{
\includegraphics[width=\linewidth,trim=50 300 50 200,clip]{VM0p01.png}}    

\\ \hline
\rotatebox[origin=c]{90}{t = 0.1} 
&
\parbox[c]{\figwidthCase em}{
\includegraphics[width=\linewidth,trim=100 550 100 500,clip]{CD0p1.png}} 
&  
%   \parbox[c]{\figwidthCase em}{
%   \includegraphics[width=\linewidth,trim=100 550 100 500,clip]{Figures/AliMani/20V/ES/ESt0p1.png}}   
%   & 
\parbox[c]{\figwidthCase em}{
\includegraphics[width=\linewidth,trim=50 300 50 200,clip]{VM0p1.png}}   


\\ \hline
\rotatebox[origin=c]{90}{t = 1.0} 
&
\parbox[c]{\figwidthCase em}{
\includegraphics[width=\linewidth,trim=100 550 100 500,clip]{CD1p0.png}}  
&
%   \parbox[c]{\figwidthCase em}{
%   \includegraphics[width=\linewidth,trim=100 550 100 500,clip]{Figures/AliMani/20V/ES/ESt1p0.png}}   
%   & 
\parbox[c]{\figwidthCase em}{
\includegraphics[width=\linewidth,trim=50 300 50 200,clip]{VM1p0.png}}   


\\ \hline
\rotatebox[origin=c]{90}{t = 5.0} & 
\parbox[c]{\figwidthCase em}{
\includegraphics[width=\linewidth,trim=100 550 100 500,clip]{CD5p0.png}} 
&
%   \parbox[c]{\figwidthCase em}{
%   \includegraphics[width=\linewidth,trim=100 550 100 500,clip]{Figures/AliMani/20V/ES/ESt5p0.png}}  
%   &  
\parbox[c]{\figwidthCase em}{
\includegraphics[width=\linewidth,trim=50 300 50 200,clip]{VM5p0.png}}  
\\ \hline
\end{tabular}
\caption{Contour plots of charge density and velocity magnitudes  at different times for $\Delta \phi = $ 20. }
\label{fig:plot20V}
\end{figure}

%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%


\begin{figure}[htb!]
\begin{tabular}{|l|l|l|l|}
\hline
& 
\parbox[c]{\figwidthCase em}{
\includegraphics[width=\linewidth,trim=100 1200 100 100,clip]{120V_CDt0pBar.png}}
& 
% \parbox[c]{\figwidthCase em}{
%   \includegraphics[width=\linewidth,trim=100 1200 100 100,clip]{Figures/AliMani/120V/ES/ESBar.png}}
%   & 
\parbox[c]{\figwidthCase em}{
\includegraphics[width=\linewidth,trim=100 1200 100 100,clip]{120V_VMbar.png}}
\\ \hline
\rotatebox[origin=c]{90}{t = 0.001} 
& \parbox[c]{\figwidthCase em}{
\includegraphics[width=\linewidth,trim=100 550 100 500,clip]{120V_CDt0p001.png}}
% &\parbox[c]{\figwidthCase em}{
%   \includegraphics[width=\linewidth,trim=100 550 100 500,clip]{Figures/AliMani/120V/ES/ESt0p001.png}}   
& \parbox[c]{\figwidthCase em}{
\includegraphics[width=\linewidth,trim=50 275 50 250,clip]{120V_VMt0p001.png}}   

\\ \hline
\rotatebox[origin=c]{90}{t = 0.005} & \parbox[c]{\figwidthCase em}{
\includegraphics[width=\linewidth,trim=100 550 100 500,clip]{120V_CDt0p005.png}} 
%   &  
%   \parbox[c]{\figwidthCase em}{
%   \includegraphics[width=\linewidth,trim=100 550 100 500,clip]{Figures/AliMani/120V/ES/ESt0p005.png}}   
& 
\parbox[c]{\figwidthCase em}{
\includegraphics[width=\linewidth,trim=50 275 50 250,clip]{120V_VMt0p005.png}}   


\\ \hline
\rotatebox[origin=c]{90}{t = 0.01} & \parbox[c]{\figwidthCase em}{
\includegraphics[width=\linewidth,trim=100 550 100 500,clip]{120V_CDt0p01.png}}  
%   &
%   \parbox[c]{\figwidthCase em}{
%   \includegraphics[width=\linewidth,trim=100 550 100 500,clip]{Figures/AliMani/120V/ES/ESt0p01.png}}   
& 
\parbox[c]{\figwidthCase em}{
\includegraphics[width=\linewidth,trim=50 275 50 250,clip]{120V_VMt0p01.png}}   


\\ \hline
\rotatebox[origin=c]{90}{t = 0.05} & 
\parbox[c]{\figwidthCase em}{
\includegraphics[width=\linewidth,trim=100 550 100 500,clip]{120V_CDt0p05.png}} 
%   &
%   \parbox[c]{\figwidthCase em}{
%   \includegraphics[width=\linewidth,trim=100 550 100 500,clip]{Figures/AliMani/120V/ES/ESt0p05.png}}  
&  
\parbox[c]{\figwidthCase em}{
\includegraphics[width=\linewidth,trim=50 275 50 250,clip]{120V_VMt0p05.png}}  


\\ \hline
\rotatebox[origin=c]{90}{t = 0.1} & 
\parbox[c]{\figwidthCase em}{
\includegraphics[width=\linewidth,trim=100 550 100 500,clip]{120V_CDt0p1.png}} 
%   &
%   \parbox[c]{\figwidthCase em}{
%   \includegraphics[width=\linewidth,trim=100 550 100 500,clip]{Figures/AliMani/120V/ES/ESt0p1.png}}  
&  
\parbox[c]{\figwidthCase em}{
\includegraphics[width=\linewidth,trim=50 275 50 250,clip]{120V_VMt0p1.png}}   
\\ \hline
\end{tabular}
\caption{Contour plots of charge density and velocity magnitudes  at different times for $\Delta \phi = $ 120. }
\label{fig:plot120V}
\end{figure}
% \end{landscape}

\begin{figure}
\begin{subfigure}{0.45\textwidth}
%		\begin{tikzpicture}
%			\begin{axis}[
%				width=1.0\textwidth, % Scale the plot to \linewidth
%				%grid=major, % Display a grid
%				%grid style={line width=.1pt, draw=gray!50}, % Set the style
%				xlabel=Time: $t$, % Set the
%				ylabel=Current density: $|j_{T}^{\pm}|$,
%				xmax = 0.1,
%				xmin = 0.0,
%				%   ymin = 0.0,
%				legend style={at={(0.17,0.95)},anchor=north west,legend columns=2}, % Put the legend below the plot
%				x tick label style={rotate=0,anchor=north},
%				xtick={0,0.02,0.04,0.06,0.08,0.10},
%				xticklabels={0,0.02,0.04,0.06,0.08,0.10}
%				]
%				\addplot [no marks] gnuplot [raw gnuplot, color = red,ultra thick] {
%					plot "20V_BulkBoundaryFlux.txt" using ($1):(-$2/8) with points
%				};
%				\addplot [no markers] gnuplot [raw gnuplot, color=blue,ultra thick] {
%					plot "120V_topCurrent.txt" using ($1):(-$2/8) with points
%				};
%				\legend{$\Delta \phi = 20$,$\Delta \phi = 120$ }
%			\end{axis}
%		\end{tikzpicture}
\includegraphics{methods_paper-figure5.pdf}
\caption{Variation of current with time}
\label{fig:current}
\end{subfigure}
\begin{subfigure}{0.45\textwidth}
%		\begin{tikzpicture}
%			\begin{axis}[
%				width=1.0\textwidth, % Scale the plot to \linewidth
%				xmode=log,
%				ymode = log,
%				%grid=major, % Display a grid
%				%grid style={line width=.1pt, draw=gray!50}, % Set the style
%				xlabel=Wall normal distance: $y$, % Set the labels
%				ylabel= Potential gradient: $\partial_y \phi$,
%				%   ymin = 5E-5,
%				xmin = 4.5E-4,
%				xmax = 1,
%				legend style={at={(0.05,0.30)},anchor=north west,legend columns=1}, % Put the legend below the plot
%				x tick label style={rotate=0,anchor=north} % Display labels sideways
%				]
%				\addplot [no markers] gnuplot [raw gnuplot, color=red,ultra thick] {
%					plot "20V_AveragedTimeStep_900.txt" using 1:4 with points
%				};
%				\addplot [no marks] gnuplot [raw gnuplot, color = blue,ultra thick] {
%					plot "120V_AveragedTimeStep_11200.txt" using 1:4 with points
%				};
%				\addplot[only marks,very thick,mark repeat=2,mark=o,mark size=2pt,red ]  table[x ={X}, y ={PotGrad} ]{20V_PotentialGrad.txt};
%				\addplot[only marks,very thick,mark repeat=2,mark=o,mark size=2pt,blue ]  table[x ={X}, y ={PotGrad} ]{120V_PotentialGrad.txt};
%				\legend{$\Delta \phi = 20$,$\Delta \phi = 120$ }
%			\end{axis}
%		\end{tikzpicture}
\includegraphics{methods_paper-figure6.pdf}
\caption{Variation of potential gradient with $y$}
\label{fig:ptVar}
\end{subfigure}
\par\bigskip
\begin{subfigure}{0.45\textwidth}
%		\begin{tikzpicture}
%			\begin{axis}[
%				width=1.0\textwidth, % Scale the plot to \linewidth
%				%grid=major, % Display a grid
%				%grid style={line width=.1pt, draw=gray!50}, % Set the style
%				xlabel=Wall normal distance:$y$, % Set the
%				ylabel=Concentration: $\Bar{c}^s$,
%				xmax = 1.0,
%				xmin = 0.0,
%				ymin = 0.0,
%				legend style={at={(0.65,0.9)},anchor=north west,legend columns=1}, % Put the legend below the plot
%				x tick label style={rotate=0,anchor=north} % Display labels sideways
%				]
%				\addplot [no marks] gnuplot [raw gnuplot, color = blue,ultra thick] {
%					plot "120V_AveragedTimeStep_11200.txt" using 1:2 with points
%				};
%				\addplot [no markers] gnuplot [raw gnuplot, color=red,ultra thick,dashed] {
%					plot "120V_AveragedTimeStep_11200.txt" using 1:3 with points
%				};
%				\addplot [no marks] gnuplot [raw gnuplot, color = blue,ultra thick] {
%					plot "20V_AveragedTimeStep_900.txt" using 1:2 with points
%				};
%				\addplot [no markers] gnuplot [raw gnuplot, color=red,ultra thick,dashed] {
%					plot "20V_AveragedTimeStep_900.txt" using 1:3 with points
%				};
%				%  \addplot [only marks, mark='x'] {
%					%   plot "Data/AliMani/20V/AliManiConc.txt" using 1:2 with points
%					%   }; 
%				\addplot[only marks,thick,mark repeat=2,mark=o,mark size=2pt ]  table[x ={X}, y ={Conc} ]{20V_AliManiConc.txt};
%				\addplot[only marks,thick,mark=o,mark size=2pt ]  table[x ={X}, y ={Conc} ]{120V_AliManiConc.txt};
%				\node[rotate=25] at (axis cs: 0.45,0.5) {$\Delta \phi = 20$};
%				\node[] at (axis cs: 0.5,0.18) {$\Delta \phi = 120$};
%				\legend{\small $c^+$ ,\small $c^-$ }
%			\end{axis}
%		\end{tikzpicture}
\includegraphics{methods_paper-figure7.pdf}
\caption{Variation of concentration with $y$}
\label{fig:cVar}
\end{subfigure}
%
\begin{subfigure}{0.45\textwidth}
%		\begin{tikzpicture}
%			\begin{axis}[
%				width=1.0\textwidth, % Scale the plot to \linewidth
%				xmode=log,
%				ymode = log,
%				%grid=major, % Display a grid
%				%grid style={line width=.1pt, draw=gray!50}, % Set the style
%				xlabel=Wall normal distance: $y$, % Set the labels
%				ylabel=Charge density: $\Bar{c}^+ - \Bar{c}^-$,
%				ymin = 5E-5,
%				xmin = 4.5E-4,
%				xmax = 1,
%				legend style={at={(0.50,0.95)},anchor=north west,legend columns=1}, % Put the legend below the plot
%				x tick label style={rotate=0,anchor=north} % Display labels sideways
%				]
%				\addplot [no markers] gnuplot [raw gnuplot, color=red,ultra thick] {
%					plot "20V_AveragedTimeStep_900.txt" using 1:5 with points
%				};
%				\addplot [no marks] gnuplot [raw gnuplot, color = blue,ultra thick] {
%					plot "120V_AveragedTimeStep_11200.txt" using 1:5 with points
%				};
%				\addplot[only marks,very thick,mark repeat=2,mark=o,mark size=2pt,blue ]  table[x ={X}, y ={ChargeDensity} ]{120V_ChargeDensity.txt};
%				\addplot[only marks, very thick,mark repeat=2,mark=o,mark size=2pt,red ]  table[x ={X}, y ={ChargeDensity} ]{20V_ChargeDensity.txt};
%				\legend{$\Delta \phi = 20$,$\Delta \phi = 120$ }
%			\end{axis}
%		\end{tikzpicture}
\includegraphics{methods_paper-figure8.pdf}
\caption{Variation of charge density  with $y$}
\label{fig:cdVar}
\end{subfigure}
\caption{ \textit{Electro-osmotic instability:} Variation of current with time (in s) for different $\Delta \phi$ is shown in ~\figref{fig:current}.  Time and X averaged variation of potential gradient ($\partial_y\phi$) (\figref{fig:ptVar}), concentration (\figref{fig:cVar}) and charge density  (\figref{fig:cdVar}) with wall normal distance $y$ for $\Delta \phi = $ 20  and 120. The dotted markers in the plot corresponds to the previous DNS results by ~\citet{druzgalski2013direct}}
\label{fig: AliManiCase}
\end{figure}{}

% \input{Files/120V}



