using System;
using System.Globalization;
using System.Windows;
using System.Windows.Data;

namespace FlareWin
{
    /// <summary>true → Visible；false → Collapsed。ConverterParameter=invert 时反转。</summary>
    public class BoolToVis : IValueConverter
    {
        public object Convert(object v, Type t, object p, CultureInfo c)
        {
            var b = v is bool bv && bv;
            if (string.Equals(p as string, "invert", StringComparison.OrdinalIgnoreCase)) b = !b;
            return b ? Visibility.Visible : Visibility.Collapsed;
        }

        public object ConvertBack(object v, Type t, object p, CultureInfo c)
            => v is Visibility vis && vis == Visibility.Visible;
    }

    /// <summary>空字符串/空集合 → Collapsed（用于欢迎页、空态提示）。</summary>
    public class EmptyToVis : IValueConverter
    {
        public object Convert(object v, Type t, object p, CultureInfo c)
        {
            var empty = v == null || (v is string s && s.Trim().Length == 0);
            if (string.Equals(p as string, "invert", StringComparison.OrdinalIgnoreCase)) empty = !empty;
            return empty ? Visibility.Visible : Visibility.Collapsed;
        }

        public object ConvertBack(object v, Type t, object p, CultureInfo c)
            => throw new NotSupportedException();
    }

    public class CountToVis : IValueConverter
    {
        public object Convert(object v, Type t, object p, CultureInfo c)
        {
            var n = v is int i ? i : 0;
            var empty = n == 0;
            if (string.Equals(p as string, "invert", StringComparison.OrdinalIgnoreCase)) empty = !empty;
            return empty ? Visibility.Visible : Visibility.Collapsed;
        }

        public object ConvertBack(object v, Type t, object p, CultureInfo c)
            => throw new NotSupportedException();
    }
}
