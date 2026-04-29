using System.Windows;
using System.Windows.Threading;
using JoystickMapper.App.ViewModels;

namespace JoystickMapper.App;

public partial class MainWindow : Window
{
    private readonly MainViewModel _viewModel = new();
    private readonly DispatcherTimer _timer;

    public MainWindow()
    {
        InitializeComponent();
        DataContext = _viewModel;
        _timer = new DispatcherTimer { Interval = TimeSpan.FromMilliseconds(80) };
        _timer.Tick += (_, _) => _viewModel.Tick();
        _timer.Start();
    }
}
